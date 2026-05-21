/*
 * gpu_duck_sw_backend.cpp
 *
 * VS2010-compatible port of SwanStation's gpu_sw_backend.cpp.
 *
 * Changes from upstream:
 *
 *   - `if constexpr` replaced with plain `if`. Every use in this file
 *     has a body that is legal under any template instantiation, so
 *     the branches the compiler can't prove dead still compile; and
 *     MSVC folds the compile-time-constant `if` away at /O2. The ABI
 *     is unchanged.
 *
 *   - Structured bindings (`const auto [r,g,b] = ...`) replaced with
 *     the out-param form of UnpackColorRGB24 / UnpackTexcoord declared
 *     in the ported header.
 *
 *   - Uniform-initialisation of unions like `VRAMPixel color{bits}` or
 *     `GPURenderCommand rc{bits}` replaced with default ctor + explicit
 *     `.bits = ...` assignment. VS2010's support for brace-init of
 *     unions with anonymous struct members is inconsistent.
 *
 *   - `= default` destructor replaced with `{}`.
 *
 *   - `constexpr DitherLUT ComputeDitherLUT()` (upstream is a C++17
 *     constexpr function whose result is stored in a static constexpr
 *     variable) replaced with a runtime-initialised file-static LUT
 *     populated once from GPU_SW_Backend::Initialize(). This also
 *     avoids the non-trivial static-ctor problem where std::array of
 *     std::array would otherwise run before main().
 *
 *   - `static constexpr FuncPtr funcs[...]` tables demoted to plain
 *     `static` (the shim strips constexpr anyway); the arrays remain
 *     immutable in practice. DrawRectangle/DrawLine template overloads
 *     renamed to *Templated so they can't clash with the non-templated
 *     virtuals.
 *
 *   - Nothing from host_display.h / system.h is used by the rasteriser
 *     proper, so those upstream includes are dropped. Display-side
 *     work happens in the separate adapter layer.
 */

#include "gpu_duck_sw_backend.h"
#include "gpu_duck_compat.h"

#include <algorithm>
#include <cstdlib> /* std::abs */

/* Toggle global de dithering (libretro option `pcsxr360_dithering`).
 * Definido en libretro_core.cpp. Cuando vale 0, el sw_backend salta el path
 * de dither aunque el juego haya seteado el bit en GP1. Util para ganar
 * fillrate o desactivar el dither en juegos donde produce artefactos. */
extern "C" int g_pcsxr_dithering;

/* True Color (24-bit shadow framebuffer). Definidos en gpu_duck_driver.cpp.
 * `g_psxVuw24` es u32* (BGR888 con A=0) de 1024x512.  Cuando active vale 1
 * Y g_psxVuw24 != NULL, ShadePixel y los path de VRAM ops escriben el valor
 * 8-bit-por-canal al shadow ademas del 5-bit a psxVuw normal.  El display
 * lo lee desde xbox_soft/draw_ok.c BlitScreen32. */
extern "C" unsigned int* g_psxVuw24;
extern "C" int           g_pcsxr_true_color_active;

/* ------------------------------------------------------------------
 * Dither LUT — runtime-initialised once.
 * Upstream did this at compile time via `constexpr`; VS2010 has none.
 * ---------------------------------------------------------------- */
static GPU_SW_Backend::DitherLUT s_dither_lut;
static bool s_dither_lut_initialized = false;

static void InitDitherLUT()
{
  if (s_dither_lut_initialized)
    return;

  for (u32 i = 0; i < DITHER_MATRIX_SIZE; i++)
  {
    for (u32 j = 0; j < DITHER_MATRIX_SIZE; j++)
    {
      for (u32 value = 0; value < GPU_SW_Backend::DITHER_LUT_SIZE; value++)
      {
        const s32 dithered_value = (static_cast<s32>(value) + DITHER_MATRIX[i][j]) >> 3;
        s_dither_lut[i][j][value] =
          static_cast<u8>((dithered_value < 0) ? 0 : ((dithered_value > 31) ? 31 : dithered_value));
      }
    }
  }

  s_dither_lut_initialized = true;
}

/* ------------------------------------------------------------------
 * Construction / lifecycle.
 * ---------------------------------------------------------------- */

GPU_SW_Backend::GPU_SW_Backend() : GPUBackend()
{
  m_vram.fill(0);
  m_vram_ptr = m_vram.data();
  InitDitherLUT();
}

GPU_SW_Backend::GPU_SW_Backend(u16* external_vram) : GPUBackend()
{
  /* Don't touch m_vram — the owned array is unused in this mode but
   * still sits in the object layout. We leave it uninitialised (it's
   * a POD-ish std::array of u16; default ctor already zero-inits on
   * MSVC value-init paths, but we never read it). Pointing m_vram_ptr
   * at the caller's buffer is all the rasteriser needs. */
  m_vram_ptr = external_vram;
  InitDitherLUT();
}

GPU_SW_Backend::~GPU_SW_Backend()
{
}

bool GPU_SW_Backend::Initialize(bool force_thread)
{
  InitDitherLUT();
  return GPUBackend::Initialize(force_thread);
}

void GPU_SW_Backend::Reset(bool clear_vram)
{
  GPUBackend::Reset(clear_vram);
  if (clear_vram)
  {
    /* Zero through m_vram_ptr so this works for both owned and
     * external-VRAM instances. */
    std::fill_n(m_vram_ptr, VRAM_WIDTH * VRAM_HEIGHT, static_cast<u16>(0));
  }
}

/* ------------------------------------------------------------------
 * Primitive dispatch.
 * ---------------------------------------------------------------- */

void GPU_SW_Backend::DrawPolygon(const GPUBackendDrawPolygonCommand* cmd)
{
  GPURenderCommand rc;
  rc.bits = cmd->rc.bits;

  /* AND con el flag global del libretro option pcsxr360_dithering: si el
   * usuario lo desactiva, todas las primitivas van al path sin dither. */
  const bool dithering_enable =
      (g_pcsxr_dithering != 0) && rc.IsDitheringEnabled() && cmd->draw_mode.dither_enable;

  const DrawTriangleFunction DrawFunction = GetDrawTriangleFunction(
    rc.shading_enable, rc.texture_enable, rc.raw_texture_enable, rc.transparency_enable, dithering_enable);

  (this->*DrawFunction)(cmd, &cmd->vertices[0], &cmd->vertices[1], &cmd->vertices[2]);
  if (rc.quad_polygon)
    (this->*DrawFunction)(cmd, &cmd->vertices[2], &cmd->vertices[1], &cmd->vertices[3]);
}

void GPU_SW_Backend::DrawRectangle(const GPUBackendDrawRectangleCommand* cmd)
{
  GPURenderCommand rc;
  rc.bits = cmd->rc.bits;

  const DrawRectangleFunction DrawFunction =
    GetDrawRectangleFunction(rc.texture_enable, rc.raw_texture_enable, rc.transparency_enable);

  (this->*DrawFunction)(cmd);
}

void GPU_SW_Backend::DrawLine(const GPUBackendDrawLineCommand* cmd)
{
  /* Mismo AND con g_pcsxr_dithering que en DrawPolygon. */
  const bool dithering_enable = (g_pcsxr_dithering != 0) && cmd->IsDitheringEnabled();
  const DrawLineFunction DrawFunction =
    GetDrawLineFunction(cmd->rc.shading_enable, cmd->rc.transparency_enable, dithering_enable);

  for (u16 i = 1; i < cmd->num_vertices; i++)
    (this->*DrawFunction)(cmd, &cmd->vertices[i - 1], &cmd->vertices[i]);
}

/* ------------------------------------------------------------------
 * ShadePixel — core per-pixel path.
 * `if constexpr` in upstream replaced with plain `if` on the template
 * bool parameter (MSVC folds this away).
 * ---------------------------------------------------------------- */

template<bool texture_enable, bool raw_texture_enable, bool transparency_enable, bool dithering_enable>
void ALWAYS_INLINE_RELEASE GPU_SW_Backend::ShadePixel(const GPUBackendDrawCommand* cmd, u32 x, u32 y, u8 color_r,
                                                      u8 color_g, u8 color_b, u8 texcoord_x, u8 texcoord_y)
{
  VRAMPixel color;
  color.bits = 0;

  /* === True-color shadow value ===
   * Cuando g_pcsxr_true_color_active != 0, computamos en paralelo el valor
   * de 8-bits por canal del pixel y lo escribimos a g_psxVuw24 al final.
   * Layout: 0x00RRGGBB (R en byte 2, G en byte 1, B en byte 0) -- el
   * formato que BlitScreen32 puede OR-ar con 0xFF000000 sin shuffle.
   * Cuando true-color esta off, el if final no entra y la variable se
   * elimina por DCE del optimizador. */
  u32 c24 = 0;

  if (texture_enable)
  {
    texcoord_x = static_cast<u8>((texcoord_x & cmd->window.and_x) | cmd->window.or_x);
    texcoord_y = static_cast<u8>((texcoord_y & cmd->window.and_y) | cmd->window.or_y);

    VRAMPixel texture_color;
    texture_color.bits = 0;

    switch (cmd->draw_mode.texture_mode)
    {
      case GPUTextureMode::Palette4Bit:
      {
        const u16 palette_value =
          GetPixel((cmd->draw_mode.GetTexturePageBaseX() + ZeroExtend32(texcoord_x / 4)) & VRAM_WIDTH_MASK,
                   (cmd->draw_mode.GetTexturePageBaseY() + ZeroExtend32(texcoord_y)) & VRAM_HEIGHT_MASK);
        const u16 palette_index = static_cast<u16>((palette_value >> ((texcoord_x % 4) * 4)) & 0x0Fu);

        texture_color.bits =
          GetPixel((cmd->palette.GetXBase() + ZeroExtend32(palette_index)) & VRAM_WIDTH_MASK, cmd->palette.GetYBase());
        break;
      }

      case GPUTextureMode::Palette8Bit:
      {
        const u16 palette_value =
          GetPixel((cmd->draw_mode.GetTexturePageBaseX() + ZeroExtend32(texcoord_x / 2)) & VRAM_WIDTH_MASK,
                   (cmd->draw_mode.GetTexturePageBaseY() + ZeroExtend32(texcoord_y)) & VRAM_HEIGHT_MASK);
        const u16 palette_index = static_cast<u16>((palette_value >> ((texcoord_x % 2) * 8)) & 0xFFu);
        texture_color.bits =
          GetPixel((cmd->palette.GetXBase() + ZeroExtend32(palette_index)) & VRAM_WIDTH_MASK, cmd->palette.GetYBase());
        break;
      }

      default:
      {
        texture_color.bits = GetPixel((cmd->draw_mode.GetTexturePageBaseX() + ZeroExtend32(texcoord_x)) & VRAM_WIDTH_MASK,
                                      (cmd->draw_mode.GetTexturePageBaseY() + ZeroExtend32(texcoord_y)) & VRAM_HEIGHT_MASK);
        break;
      }
    }

    if (texture_color.bits == 0)
      return;

    if (raw_texture_enable)
    {
      color.bits = texture_color.bits;

      /* True-color: texture-color de 5-bit -> 8-bit via bit-replication.
       * Para raw texture no hay modulacion, el color 8-bit es simplemente
       * el texel expandido a 256 niveles. */
      if (g_pcsxr_true_color_active)
      {
        const u32 tr5 = (texture_color.bits >>  0) & 0x1F;
        const u32 tg5 = (texture_color.bits >>  5) & 0x1F;
        const u32 tb5 = (texture_color.bits >> 10) & 0x1F;
        c24 = (((tr5 << 3) | (tr5 >> 2)) << 16)
            | (((tg5 << 3) | (tg5 >> 2)) <<  8)
            | (((tb5 << 3) | (tb5 >> 2)) <<  0);
      }
    }
    else
    {
      const u32 dither_y = (dithering_enable) ? (y & 3u) : 2u;
      const u32 dither_x = (dithering_enable) ? (x & 3u) : 3u;

      const u16 tr = static_cast<u16>(texture_color.r);
      const u16 tg = static_cast<u16>(texture_color.g);
      const u16 tb = static_cast<u16>(texture_color.b);

      color.bits = static_cast<u16>(
        (ZeroExtend16(s_dither_lut[dither_y][dither_x][(tr * static_cast<u16>(color_r)) >> 4]) << 0) |
        (ZeroExtend16(s_dither_lut[dither_y][dither_x][(tg * static_cast<u16>(color_g)) >> 4]) << 5) |
        (ZeroExtend16(s_dither_lut[dither_y][dither_x][(tb * static_cast<u16>(color_b)) >> 4]) << 10) |
        (texture_color.bits & 0x8000u));

      /* True-color: modulacion 8-bit.
       *
       * CRITICAL: la formula DEBE satisfacer quantize_to_5bit(8bit_value)
       * == 5bit_value de psxVuw, porque BlitScreen32 hace un sanity check
       * per-pixel (quantize(shadow) ?= vram) y si falla reexpande desde
       * psxVuw via bit-replication, perdiendo los 8 bits.
       *
       * El path 5-bit (LUT) hace: ((tr_5 * color_r) >> 4) >> 3 clamp 31
       *                        = (tr_5 * color_r) >> 7 clamp 31
       *
       * Para que quantize(8bit) >> 3 == ese 5bit valor exactamente,
       * usamos el valor intermedio del LUT (pre-quantize):
       *   c24 = (tr_5 * color_r) >> 4 clamp 255
       *
       * Esto es exactamente lo que el LUT consume; el >>3 de quantize
       * recupera el 5-bit oficial.  Y como tr_5 y color_r varian
       * suavemente (Gouraud + texture sample), el valor 8-bit tambien:
       * sin banding por cuantizacion.
       *
       * Una formulacion previa usaba bit-replicate(tr_5) y >>7 (i.e. la
       * "matematica 8-bit teoricamente correcta" del PSX), pero diferia
       * del LUT por ~3% por la no-linealidad de bit-replicate, lo que
       * rompia el sanity check y producia banding identico al de
       * dither-off.  Esta nueva formulacion lo arregla. */
      if (g_pcsxr_true_color_active)
      {
        u32 r8 = (tr * static_cast<u32>(color_r)) >> 4;
        u32 g8 = (tg * static_cast<u32>(color_g)) >> 4;
        u32 b8 = (tb * static_cast<u32>(color_b)) >> 4;
        if (r8 > 255) r8 = 255;
        if (g8 > 255) g8 = 255;
        if (b8 > 255) b8 = 255;
        c24 = (r8 << 16) | (g8 << 8) | b8;
      }
    }
  }
  else
  {
    const u32 dither_y = (dithering_enable) ? (y & 3u) : 2u;
    const u32 dither_x = (dithering_enable) ? (x & 3u) : 3u;

    color.bits = static_cast<u16>(
      (ZeroExtend16(s_dither_lut[dither_y][dither_x][color_r]) << 0) |
      (ZeroExtend16(s_dither_lut[dither_y][dither_x][color_g]) << 5) |
      (ZeroExtend16(s_dither_lut[dither_y][dither_x][color_b]) << 10) |
      (transparency_enable ? 0x8000u : 0u));

    /* True-color: pure shaded - color_r/g/b ya estan en 8-bit. */
    if (g_pcsxr_true_color_active)
    {
      c24 = (static_cast<u32>(color_r) << 16)
          | (static_cast<u32>(color_g) <<  8)
          |  static_cast<u32>(color_b);
    }
  }

  VRAMPixel bg_color;
  bg_color.bits = GetPixel(x, y);

  if (transparency_enable)
  {
    if ((color.bits & 0x8000u) || !texture_enable)
    {
      u32 bg_bits = ZeroExtend32(bg_color.bits);
      u32 fg_bits = ZeroExtend32(color.bits);
      switch (cmd->draw_mode.transparency_mode)
      {
        case GPUTransparencyMode::HalfBackgroundPlusHalfForeground:
        {
          bg_bits |= 0x8000u;
          color.bits = Truncate16(((fg_bits + bg_bits) - ((fg_bits ^ bg_bits) & 0x0421u)) >> 1);
          break;
        }

        case GPUTransparencyMode::BackgroundPlusForeground:
        {
          bg_bits &= ~0x8000u;

          const u32 sum = fg_bits + bg_bits;
          const u32 carry = (sum - ((fg_bits ^ bg_bits) & 0x8421u)) & 0x8420u;

          color.bits = Truncate16((sum - carry) | (carry - (carry >> 5)));
          break;
        }

        case GPUTransparencyMode::BackgroundMinusForeground:
        {
          bg_bits |= 0x8000u;
          fg_bits &= ~0x8000u;

          const u32 diff = bg_bits - fg_bits + 0x108420u;
          const u32 borrow = (diff - ((bg_bits ^ fg_bits) & 0x108420u)) & 0x108420u;

          color.bits = Truncate16((diff - borrow) & (borrow - (borrow >> 5)));
          break;
        }

        case GPUTransparencyMode::BackgroundPlusQuarterForeground:
        {
          bg_bits &= ~0x8000u;
          fg_bits = ((fg_bits >> 2) & 0x1CE7u) | 0x8000u;

          const u32 sum = fg_bits + bg_bits;
          const u32 carry = (sum - ((fg_bits ^ bg_bits) & 0x8421u)) & 0x8420u;

          color.bits = Truncate16((sum - carry) | (carry - (carry >> 5)));
          break;
        }
      }

      /* === True-color: 8-bit transparency blend ===
       * El blend nativo del PSX (arriba, en 5-bit) produce banding visible
       * sobre gradientes suaves -- ESTE es el motivo por el que la niebla
       * de Silent Hill mantiene su patron de "arcoiris" cuando se quita el
       * dither: las primitivas de niebla son semi-transparentes, asi que
       * cada pixel pasa por la formula de blend cuantizada a 5-bit.
       *
       * Aqui replicamos las 4 formulas en 8-bit, leyendo el background del
       * shadow (con sanity check vs psxVuw para detectar writes externos
       * que no pasaron por gpu_duck), y produciendo un foreground blend
       * con precision completa de 8-bit.  El resultado se queda en c24,
       * que sera el valor que se escriba al shadow al final. */
      if (g_pcsxr_true_color_active && g_psxVuw24)
      {
        const u32 bg_idx = VRAM_WIDTH * y + x;
        u32 bg24 = g_psxVuw24[bg_idx];
        /* Verificar sync con psxVuw con tolerancia +-1 LSB por canal.
         *
         * CRITICAL: comparacion exacta NO funciona aqui.  Cuando un pixel
         * ya ha sido modificado por una primitiva transparente previa,
         * su 8-bit shadow y su 5-bit psxVuw difieren legitimamente por
         * hasta 1 LSB (el blend 5-bit cuantiza fg/bg antes de mezclar,
         * el 8-bit no).  Una comparacion estricta rechazaria casi todos
         * los pixels de niebla compuesta (capas de transparencia sobre
         * fondo previamente blendeado) y bit-replicaria desde 5-bit,
         * tirando la precision 8-bit que es el ENTERO objetivo del modo
         * true color.
         *
         * Tolerancia +-1 LSB acepta drift de cuantizacion legitimo y
         * sigue detectando escrituras externas a psxVuw (loadImage,
         * MDEC, etc.) que normalmente difieren por mucho mas. */
        const u32 sh_r5 = (bg24 >> 19) & 0x1F;
        const u32 sh_g5 = (bg24 >> 11) & 0x1F;
        const u32 sh_b5 = (bg24 >>  3) & 0x1F;
        const u32 bg_r5 = (bg_color.bits >>  0) & 0x1F;
        const u32 bg_g5 = (bg_color.bits >>  5) & 0x1F;
        const u32 bg_b5 = (bg_color.bits >> 10) & 0x1F;
        s32 dr = (s32)sh_r5 - (s32)bg_r5; if (dr < 0) dr = -dr;
        s32 dg = (s32)sh_g5 - (s32)bg_g5; if (dg < 0) dg = -dg;
        s32 db = (s32)sh_b5 - (s32)bg_b5; if (db < 0) db = -db;
        if (dr > 1 || dg > 1 || db > 1)
        {
          bg24 = (((bg_r5 << 3) | (bg_r5 >> 2)) << 16)
               | (((bg_g5 << 3) | (bg_g5 >> 2)) <<  8)
               | (((bg_b5 << 3) | (bg_b5 >> 2)) <<  0);
        }

        const u32 bgr = (bg24 >> 16) & 0xFFu;
        const u32 bgg = (bg24 >>  8) & 0xFFu;
        const u32 bgb = (bg24 >>  0) & 0xFFu;
        const u32 fgr = (c24  >> 16) & 0xFFu;
        const u32 fgg = (c24  >>  8) & 0xFFu;
        const u32 fgb = (c24  >>  0) & 0xFFu;

        u32 nr, ng, nb;
        switch (cmd->draw_mode.transparency_mode)
        {
          case GPUTransparencyMode::HalfBackgroundPlusHalfForeground:
            nr = (bgr + fgr) >> 1;
            ng = (bgg + fgg) >> 1;
            nb = (bgb + fgb) >> 1;
            break;
          case GPUTransparencyMode::BackgroundPlusForeground:
            nr = bgr + fgr; if (nr > 255) nr = 255;
            ng = bgg + fgg; if (ng > 255) ng = 255;
            nb = bgb + fgb; if (nb > 255) nb = 255;
            break;
          case GPUTransparencyMode::BackgroundMinusForeground:
            nr = (bgr > fgr) ? (bgr - fgr) : 0;
            ng = (bgg > fgg) ? (bgg - fgg) : 0;
            nb = (bgb > fgb) ? (bgb - fgb) : 0;
            break;
          case GPUTransparencyMode::BackgroundPlusQuarterForeground:
            nr = bgr + (fgr >> 2); if (nr > 255) nr = 255;
            ng = bgg + (fgg >> 2); if (ng > 255) ng = 255;
            nb = bgb + (fgb >> 2); if (nb > 255) nb = 255;
            break;
          default:
            nr = fgr; ng = fgg; nb = fgb;
            break;
        }
        c24 = (nr << 16) | (ng << 8) | nb;
      }

      if (!texture_enable)
        color.bits &= ~0x8000u;
    }
  }

  const u16 mask_and = cmd->params.GetMaskAND();
  if ((bg_color.bits & mask_and) != 0)
    return;

  SetPixel(x, y, static_cast<u16>(color.bits | cmd->params.GetMaskOR()));

  /* True-color: write shadow buffer.  c24 ya tiene el valor correcto en
   * 8-bit por canal -- ya sea computado directamente en ShadePixel para
   * pixels opacos (raw/modulated texture o pure shaded) o calculado con
   * el 8-bit transparency blend arriba para pixels semi-transparentes.
   * Cero quantizacion a 5-bit en el path true-color, asi que no hay
   * banding en gradientes ni en la niebla de Silent Hill. */
  if (g_pcsxr_true_color_active && g_psxVuw24)
  {
    g_psxVuw24[VRAM_WIDTH * y + x] = c24;
  }
}

/* ------------------------------------------------------------------
 * Rectangle rasterisation (templated on texture/raw/transparency).
 * Renamed from upstream's `DrawRectangle` overload to
 * `DrawRectangleTemplated` to avoid ambiguity with the virtual.
 * Structured bindings -> out-params.
 * ---------------------------------------------------------------- */

template<bool texture_enable, bool raw_texture_enable, bool transparency_enable>
void GPU_SW_Backend::DrawRectangleTemplated(const GPUBackendDrawRectangleCommand* cmd)
{
  const s32 origin_x = cmd->x;
  const s32 origin_y = cmd->y;

  u8 r, g, b;
  UnpackColorRGB24(cmd->color, r, g, b);

  u8 origin_texcoord_x, origin_texcoord_y;
  UnpackTexcoord(cmd->texcoord, origin_texcoord_x, origin_texcoord_y);

  for (u32 offset_y = 0; offset_y < cmd->height; offset_y++)
  {
    const s32 y = origin_y + static_cast<s32>(offset_y);
    if (y < static_cast<s32>(m_drawing_area.top) || y > static_cast<s32>(m_drawing_area.bottom) ||
        (cmd->params.interlaced_rendering &&
         cmd->params.active_line_lsb == (Truncate8(static_cast<u32>(y)) & 1u)))
    {
      continue;
    }

    const u8 texcoord_y = Truncate8(ZeroExtend32(origin_texcoord_y) + offset_y);

    for (u32 offset_x = 0; offset_x < cmd->width; offset_x++)
    {
      const s32 x = origin_x + static_cast<s32>(offset_x);
      if (x < static_cast<s32>(m_drawing_area.left) || x > static_cast<s32>(m_drawing_area.right))
        continue;

      const u8 texcoord_x = Truncate8(ZeroExtend32(origin_texcoord_x) + offset_x);

      ShadePixel<texture_enable, raw_texture_enable, transparency_enable, false>(
        cmd, static_cast<u32>(x), static_cast<u32>(y), r, g, b, texcoord_x, texcoord_y);
    }
  }
}

/* ------------------------------------------------------------------
 * Polygon + line rasterisation — ported from Mednafen via SwanStation.
 * ---------------------------------------------------------------- */

#define COORD_FBS 12
#define COORD_MF_INT(n) ((n) << COORD_FBS)
#define COORD_POST_PADDING 12

static ALWAYS_INLINE_RELEASE s64 MakePolyXFP(s32 x)
{
  return (static_cast<s64>(static_cast<u64>(x) << 32)) + ((1LL << 32) - (1 << 11));
}

static ALWAYS_INLINE_RELEASE s64 MakePolyXFPStep(s32 dx, s32 dy)
{
  s64 ret;
  s64 dx_ex = static_cast<s64>(static_cast<u64>(dx) << 32);

  if (dx_ex < 0)
    dx_ex -= dy - 1;

  if (dx_ex > 0)
    dx_ex += dy - 1;

  ret = dx_ex / dy;
  return ret;
}

static ALWAYS_INLINE_RELEASE s32 GetPolyXFP_Int(s64 xfp)
{
  return static_cast<s32>(xfp >> 32);
}

template<bool shading_enable, bool texture_enable>
bool ALWAYS_INLINE_RELEASE GPU_SW_Backend::CalcIDeltas(i_deltas& idl,
                                                       const GPUBackendDrawPolygonCommand::Vertex* A,
                                                       const GPUBackendDrawPolygonCommand::Vertex* B,
                                                       const GPUBackendDrawPolygonCommand::Vertex* C)
{
#define CALCIS(xx, yy) (((B->xx - A->xx) * (C->yy - B->yy)) - ((C->xx - B->xx) * (B->yy - A->yy)))

  s32 denom = CALCIS(x, y);

  if (!denom)
    return false;

  if (shading_enable)
  {
    idl.dr_dx = static_cast<u32>(CALCIS(r, y) * (1 << COORD_FBS) / denom) << COORD_POST_PADDING;
    idl.dr_dy = static_cast<u32>(CALCIS(x, r) * (1 << COORD_FBS) / denom) << COORD_POST_PADDING;

    idl.dg_dx = static_cast<u32>(CALCIS(g, y) * (1 << COORD_FBS) / denom) << COORD_POST_PADDING;
    idl.dg_dy = static_cast<u32>(CALCIS(x, g) * (1 << COORD_FBS) / denom) << COORD_POST_PADDING;

    idl.db_dx = static_cast<u32>(CALCIS(b, y) * (1 << COORD_FBS) / denom) << COORD_POST_PADDING;
    idl.db_dy = static_cast<u32>(CALCIS(x, b) * (1 << COORD_FBS) / denom) << COORD_POST_PADDING;
  }

  if (texture_enable)
  {
    idl.du_dx = static_cast<u32>(CALCIS(u, y) * (1 << COORD_FBS) / denom) << COORD_POST_PADDING;
    idl.du_dy = static_cast<u32>(CALCIS(x, u) * (1 << COORD_FBS) / denom) << COORD_POST_PADDING;

    idl.dv_dx = static_cast<u32>(CALCIS(v, y) * (1 << COORD_FBS) / denom) << COORD_POST_PADDING;
    idl.dv_dy = static_cast<u32>(CALCIS(x, v) * (1 << COORD_FBS) / denom) << COORD_POST_PADDING;
  }

  return true;

#undef CALCIS
}

template<bool shading_enable, bool texture_enable>
void ALWAYS_INLINE_RELEASE GPU_SW_Backend::AddIDeltas_DX(i_group& ig, const i_deltas& idl, u32 count /*= 1*/)
{
  if (shading_enable)
  {
    ig.r += idl.dr_dx * count;
    ig.g += idl.dg_dx * count;
    ig.b += idl.db_dx * count;
  }

  if (texture_enable)
  {
    ig.u += idl.du_dx * count;
    ig.v += idl.dv_dx * count;
  }
}

template<bool shading_enable, bool texture_enable>
void ALWAYS_INLINE_RELEASE GPU_SW_Backend::AddIDeltas_DY(i_group& ig, const i_deltas& idl, u32 count /*= 1*/)
{
  if (shading_enable)
  {
    ig.r += idl.dr_dy * count;
    ig.g += idl.dg_dy * count;
    ig.b += idl.db_dy * count;
  }

  if (texture_enable)
  {
    ig.u += idl.du_dy * count;
    ig.v += idl.dv_dy * count;
  }
}

template<bool shading_enable, bool texture_enable, bool raw_texture_enable, bool transparency_enable,
         bool dithering_enable>
void GPU_SW_Backend::DrawSpan(const GPUBackendDrawPolygonCommand* cmd, s32 y, s32 x_start, s32 x_bound, i_group ig,
                              const i_deltas& idl)
{
  if (cmd->params.interlaced_rendering && cmd->params.active_line_lsb == (Truncate8(static_cast<u32>(y)) & 1u))
    return;

  s32 x_ig_adjust = x_start;
  s32 w = x_bound - x_start;
  s32 x = TruncateGPUVertexPosition(x_start);

  if (x < static_cast<s32>(m_drawing_area.left))
  {
    const s32 delta = static_cast<s32>(m_drawing_area.left) - x;
    x_ig_adjust += delta;
    x += delta;
    w -= delta;
  }

  if ((x + w) > (static_cast<s32>(m_drawing_area.right) + 1))
    w = static_cast<s32>(m_drawing_area.right) + 1 - x;

  if (w <= 0)
    return;

  AddIDeltas_DX<shading_enable, texture_enable>(ig, idl, static_cast<u32>(x_ig_adjust));
  AddIDeltas_DY<shading_enable, texture_enable>(ig, idl, static_cast<u32>(y));

  do
  {
    const u32 r = ig.r >> (COORD_FBS + COORD_POST_PADDING);
    const u32 g = ig.g >> (COORD_FBS + COORD_POST_PADDING);
    const u32 b = ig.b >> (COORD_FBS + COORD_POST_PADDING);
    const u32 u = ig.u >> (COORD_FBS + COORD_POST_PADDING);
    const u32 v = ig.v >> (COORD_FBS + COORD_POST_PADDING);

    ShadePixel<texture_enable, raw_texture_enable, transparency_enable, dithering_enable>(
      cmd, static_cast<u32>(x), static_cast<u32>(y), Truncate8(r), Truncate8(g), Truncate8(b), Truncate8(u),
      Truncate8(v));

    x++;
    AddIDeltas_DX<shading_enable, texture_enable>(ig, idl);
  } while (--w > 0);
}

template<bool shading_enable, bool texture_enable, bool raw_texture_enable, bool transparency_enable,
         bool dithering_enable>
void GPU_SW_Backend::DrawTriangle(const GPUBackendDrawPolygonCommand* cmd,
                                  const GPUBackendDrawPolygonCommand::Vertex* v0,
                                  const GPUBackendDrawPolygonCommand::Vertex* v1,
                                  const GPUBackendDrawPolygonCommand::Vertex* v2)
{
  u32 core_vertex;
  {
    u32 cvtemp = 0;

    if (v1->x <= v0->x)
    {
      if (v2->x <= v1->x)
        cvtemp = (1 << 2);
      else
        cvtemp = (1 << 1);
    }
    else if (v2->x < v0->x)
      cvtemp = (1 << 2);
    else
      cvtemp = (1 << 0);

    if (v2->y < v1->y)
    {
      std::swap(v2, v1);
      cvtemp = ((cvtemp >> 1) & 0x2) | ((cvtemp << 1) & 0x4) | (cvtemp & 0x1);
    }

    if (v1->y < v0->y)
    {
      std::swap(v1, v0);
      cvtemp = ((cvtemp >> 1) & 0x1) | ((cvtemp << 1) & 0x2) | (cvtemp & 0x4);
    }

    if (v2->y < v1->y)
    {
      std::swap(v2, v1);
      cvtemp = ((cvtemp >> 1) & 0x2) | ((cvtemp << 1) & 0x4) | (cvtemp & 0x1);
    }

    core_vertex = cvtemp >> 1;
  }

  if (v0->y == v2->y)
    return;

  if (static_cast<u32>(std::abs(v2->x - v0->x)) >= static_cast<u32>(MAX_PRIMITIVE_WIDTH) ||
      static_cast<u32>(std::abs(v2->x - v1->x)) >= static_cast<u32>(MAX_PRIMITIVE_WIDTH) ||
      static_cast<u32>(std::abs(v1->x - v0->x)) >= static_cast<u32>(MAX_PRIMITIVE_WIDTH) ||
      static_cast<u32>(v2->y - v0->y) >= static_cast<u32>(MAX_PRIMITIVE_HEIGHT))
  {
    return;
  }

  const s64 base_coord = MakePolyXFP(v0->x);
  const s64 base_step = MakePolyXFPStep((v2->x - v0->x), (v2->y - v0->y));
  s64 bound_coord_us;
  s64 bound_coord_ls;
  bool right_facing;

  if (v1->y == v0->y)
  {
    bound_coord_us = 0;
    right_facing = (v1->x > v0->x);
  }
  else
  {
    bound_coord_us = MakePolyXFPStep((v1->x - v0->x), (v1->y - v0->y));
    right_facing = (bound_coord_us > base_step);
  }

  if (v2->y == v1->y)
    bound_coord_ls = 0;
  else
    bound_coord_ls = MakePolyXFPStep((v2->x - v1->x), (v2->y - v1->y));

  i_deltas idl;
  if (!CalcIDeltas<shading_enable, texture_enable>(idl, v0, v1, v2))
    return;

  const GPUBackendDrawPolygonCommand::Vertex* vertices[3];
  vertices[0] = v0;
  vertices[1] = v1;
  vertices[2] = v2;

  i_group ig;
  ig.u = 0; ig.v = 0; ig.r = 0; ig.g = 0; ig.b = 0;

  if (texture_enable)
  {
    ig.u = (static_cast<u32>(COORD_MF_INT(vertices[core_vertex]->u)) + (1u << (COORD_FBS - 1))) << COORD_POST_PADDING;
    ig.v = (static_cast<u32>(COORD_MF_INT(vertices[core_vertex]->v)) + (1u << (COORD_FBS - 1))) << COORD_POST_PADDING;
  }

  ig.r = (static_cast<u32>(COORD_MF_INT(vertices[core_vertex]->r)) + (1u << (COORD_FBS - 1))) << COORD_POST_PADDING;
  ig.g = (static_cast<u32>(COORD_MF_INT(vertices[core_vertex]->g)) + (1u << (COORD_FBS - 1))) << COORD_POST_PADDING;
  ig.b = (static_cast<u32>(COORD_MF_INT(vertices[core_vertex]->b)) + (1u << (COORD_FBS - 1))) << COORD_POST_PADDING;

  AddIDeltas_DX<shading_enable, texture_enable>(ig, idl, static_cast<u32>(-vertices[core_vertex]->x));
  AddIDeltas_DY<shading_enable, texture_enable>(ig, idl, static_cast<u32>(-vertices[core_vertex]->y));

  struct TriangleHalf
  {
    u64 x_coord[2];
    u64 x_step[2];
    s32 y_coord;
    s32 y_bound;
    bool dec_mode;
  };
  TriangleHalf tripart[2];

  u32 vo = 0;
  u32 vp = 0;
  if (core_vertex != 0)
    vo = 1;
  if (core_vertex == 2)
    vp = 3;

  {
    TriangleHalf* tp = &tripart[vo];
    tp->y_coord = vertices[0 ^ vo]->y;
    tp->y_bound = vertices[1 ^ vo]->y;
    tp->x_coord[right_facing ? 1 : 0] = static_cast<u64>(MakePolyXFP(vertices[0 ^ vo]->x));
    tp->x_step[right_facing ? 1 : 0]  = static_cast<u64>(bound_coord_us);
    tp->x_coord[right_facing ? 0 : 1] =
      static_cast<u64>(base_coord + ((vertices[vo]->y - vertices[0]->y) * base_step));
    tp->x_step[right_facing ? 0 : 1]  = static_cast<u64>(base_step);
    tp->dec_mode = (vo != 0);
  }

  {
    TriangleHalf* tp = &tripart[vo ^ 1];
    tp->y_coord = vertices[1 ^ vp]->y;
    tp->y_bound = vertices[2 ^ vp]->y;
    tp->x_coord[right_facing ? 1 : 0] = static_cast<u64>(MakePolyXFP(vertices[1 ^ vp]->x));
    tp->x_step[right_facing ? 1 : 0]  = static_cast<u64>(bound_coord_ls);
    tp->x_coord[right_facing ? 0 : 1] =
      static_cast<u64>(base_coord + ((vertices[1 ^ vp]->y - vertices[0]->y) * base_step));
    tp->x_step[right_facing ? 0 : 1]  = static_cast<u64>(base_step);
    tp->dec_mode = (vp != 0);
  }

  for (u32 i = 0; i < 2; i++)
  {
    s32 yi = tripart[i].y_coord;
    const s32 yb = tripart[i].y_bound;

    u64 lc = tripart[i].x_coord[0];
    const u64 ls = tripart[i].x_step[0];

    u64 rc = tripart[i].x_coord[1];
    const u64 rs = tripart[i].x_step[1];

    if (tripart[i].dec_mode)
    {
      while (yi > yb)
      {
        yi--;
        lc -= ls;
        rc -= rs;

        const s32 y = TruncateGPUVertexPosition(yi);

        if (y < static_cast<s32>(m_drawing_area.top))
          break;

        if (y > static_cast<s32>(m_drawing_area.bottom))
          continue;

        DrawSpan<shading_enable, texture_enable, raw_texture_enable, transparency_enable, dithering_enable>(
          cmd, yi, GetPolyXFP_Int(static_cast<s64>(lc)), GetPolyXFP_Int(static_cast<s64>(rc)), ig, idl);
      }
    }
    else
    {
      while (yi < yb)
      {
        const s32 y = TruncateGPUVertexPosition(yi);

        if (y > static_cast<s32>(m_drawing_area.bottom))
          break;

        if (y >= static_cast<s32>(m_drawing_area.top))
        {
          DrawSpan<shading_enable, texture_enable, raw_texture_enable, transparency_enable, dithering_enable>(
            cmd, yi, GetPolyXFP_Int(static_cast<s64>(lc)), GetPolyXFP_Int(static_cast<s64>(rc)), ig, idl);
        }

        yi++;
        lc += ls;
        rc += rs;
      }
    }
  }
}

/* ------------------------------------------------------------------
 * Line rasterisation.
 * ---------------------------------------------------------------- */

static const int Line_XY_FractBits  = 32;
static const int Line_RGB_FractBits = 12;

struct line_fxp_coord
{
  u64 x, y;
  u32 r, g, b;
};

struct line_fxp_step
{
  s64 dx_dk, dy_dk;
  s32 dr_dk, dg_dk, db_dk;
};

static ALWAYS_INLINE_RELEASE s64 LineDivide(s64 delta, s32 dk)
{
  delta = static_cast<s64>(static_cast<u64>(delta) << Line_XY_FractBits);

  if (delta < 0)
    delta -= dk - 1;
  if (delta > 0)
    delta += dk - 1;

  return delta / dk;
}

template<bool shading_enable, bool transparency_enable, bool dithering_enable>
void GPU_SW_Backend::DrawLineTemplated(const GPUBackendDrawLineCommand* cmd,
                                       const GPUBackendDrawLineCommand::Vertex* p0,
                                       const GPUBackendDrawLineCommand::Vertex* p1)
{
  const s32 i_dx = std::abs(p1->x - p0->x);
  const s32 i_dy = std::abs(p1->y - p0->y);
  const s32 k = (i_dx > i_dy) ? i_dx : i_dy;
  if (i_dx >= MAX_PRIMITIVE_WIDTH || i_dy >= MAX_PRIMITIVE_HEIGHT)
    return;

  if (p0->x >= p1->x && k > 0)
    std::swap(p0, p1);

  line_fxp_step step;
  step.dx_dk = 0; step.dy_dk = 0;
  step.dr_dk = 0; step.dg_dk = 0; step.db_dk = 0;

  if (k != 0)
  {
    step.dx_dk = LineDivide(p1->x - p0->x, k);
    step.dy_dk = LineDivide(p1->y - p0->y, k);

    if (shading_enable)
    {
      step.dr_dk = static_cast<s32>(static_cast<u32>(p1->r - p0->r) << Line_RGB_FractBits) / k;
      step.dg_dk = static_cast<s32>(static_cast<u32>(p1->g - p0->g) << Line_RGB_FractBits) / k;
      step.db_dk = static_cast<s32>(static_cast<u32>(p1->b - p0->b) << Line_RGB_FractBits) / k;
    }
  }

  line_fxp_coord cur_point;
  cur_point.x = (static_cast<u64>(p0->x) << Line_XY_FractBits) | (1ULL << (Line_XY_FractBits - 1));
  cur_point.y = (static_cast<u64>(p0->y) << Line_XY_FractBits) | (1ULL << (Line_XY_FractBits - 1));
  cur_point.r = 0; cur_point.g = 0; cur_point.b = 0;

  cur_point.x -= 1024;

  if (step.dy_dk < 0)
    cur_point.y -= 1024;

  if (shading_enable)
  {
    cur_point.r = (static_cast<u32>(p0->r) << Line_RGB_FractBits) | (1u << (Line_RGB_FractBits - 1));
    cur_point.g = (static_cast<u32>(p0->g) << Line_RGB_FractBits) | (1u << (Line_RGB_FractBits - 1));
    cur_point.b = (static_cast<u32>(p0->b) << Line_RGB_FractBits) | (1u << (Line_RGB_FractBits - 1));
  }

  for (s32 i = 0; i <= k; i++)
  {
    const s32 x = static_cast<s32>((cur_point.x >> Line_XY_FractBits) & 2047);
    const s32 y = static_cast<s32>((cur_point.y >> Line_XY_FractBits) & 2047);

    if ((!cmd->params.interlaced_rendering ||
         cmd->params.active_line_lsb != (Truncate8(static_cast<u32>(y)) & 1u)) &&
        x >= static_cast<s32>(m_drawing_area.left) && x <= static_cast<s32>(m_drawing_area.right) &&
        y >= static_cast<s32>(m_drawing_area.top)  && y <= static_cast<s32>(m_drawing_area.bottom))
    {
      const u8 r = shading_enable ? static_cast<u8>(cur_point.r >> Line_RGB_FractBits) : p0->r;
      const u8 g = shading_enable ? static_cast<u8>(cur_point.g >> Line_RGB_FractBits) : p0->g;
      const u8 b = shading_enable ? static_cast<u8>(cur_point.b >> Line_RGB_FractBits) : p0->b;

      ShadePixel<false, false, transparency_enable, dithering_enable>(
        cmd, static_cast<u32>(x), static_cast<u32>(y), r, g, b, 0, 0);
    }

    cur_point.x += step.dx_dk;
    cur_point.y += step.dy_dk;

    if (shading_enable)
    {
      cur_point.r += step.dr_dk;
      cur_point.g += step.dg_dk;
      cur_point.b += step.db_dk;
    }
  }
}

/* ------------------------------------------------------------------
 * VRAM operations.
 * ---------------------------------------------------------------- */

void GPU_SW_Backend::FillVRAM(u32 x, u32 y, u32 width, u32 height, u32 color, GPUBackendCommandParameters params)
{
  /* color16 is the host-native 5551 value. VRAM memory is in PSX-LE
   * byte order (shared with psxVuw), so on BE we byteswap once here
   * and then all writes below drop the swapped value in directly. */
  const u16 color16 = VRAMSwap(VRAMRGBA8888ToRGBA5551(color));

  /* True-color shadow: el `color` original (param 32-bit) viene en RGBA8888
   * desde GPU_FillVRAM.  Lo convertimos a nuestro layout 0x00RRGGBB.  El
   * input es RGBA con R en byte 0, G en byte 1, B en byte 2 (PSX GP0). */
  const u32 c24 = ((color & 0xFFu) << 16)        /* R */
                | (((color >> 8) & 0xFFu) << 8)  /* G */
                | ((color >> 16) & 0xFFu);       /* B */

  if ((x + width) <= VRAM_WIDTH && !params.interlaced_rendering)
  {
    for (u32 yoffs = 0; yoffs < height; yoffs++)
    {
      const u32 row = (y + yoffs) & VRAM_HEIGHT_MASK;
      std::fill_n(&m_vram_ptr[row * VRAM_WIDTH + x], width, color16);
      if (g_pcsxr_true_color_active && g_psxVuw24)
        std::fill_n(&g_psxVuw24[row * VRAM_WIDTH + x], width, c24);
    }
  }
  else if (params.interlaced_rendering)
  {
    const u32 active_field = static_cast<u32>(params.active_line_lsb);
    for (u32 yoffs = 0; yoffs < height; yoffs++)
    {
      const u32 row = (y + yoffs) & VRAM_HEIGHT_MASK;
      if ((row & 1u) == active_field)
        continue;

      u16* row_ptr = &m_vram_ptr[row * VRAM_WIDTH];
      u32* row_ptr24 = (g_pcsxr_true_color_active && g_psxVuw24)
                       ? &g_psxVuw24[row * VRAM_WIDTH] : 0;
      for (u32 xoffs = 0; xoffs < width; xoffs++)
      {
        const u32 col = (x + xoffs) & VRAM_WIDTH_MASK;
        row_ptr[col] = color16;
        if (row_ptr24) row_ptr24[col] = c24;
      }
    }
  }
  else
  {
    for (u32 yoffs = 0; yoffs < height; yoffs++)
    {
      const u32 row = (y + yoffs) & VRAM_HEIGHT_MASK;
      u16* row_ptr = &m_vram_ptr[row * VRAM_WIDTH];
      u32* row_ptr24 = (g_pcsxr_true_color_active && g_psxVuw24)
                       ? &g_psxVuw24[row * VRAM_WIDTH] : 0;
      for (u32 xoffs = 0; xoffs < width; xoffs++)
      {
        const u32 col = (x + xoffs) & VRAM_WIDTH_MASK;
        row_ptr[col] = color16;
        if (row_ptr24) row_ptr24[col] = c24;
      }
    }
  }
}

void GPU_SW_Backend::UpdateVRAM(u32 x, u32 y, u32 width, u32 height, const void* data,
                                GPUBackendCommandParameters params)
{
  /* Note: in the pcsxr-360 port this path is dead code — LoadImage is
   * delegated to xbox_soft's primLoadImage, which streams pixels into
   * psxVuw directly via PUTLE16. Kept correct-on-BE anyway so the
   * backend is self-consistent if anything ever calls it.
   *
   * Contract: `data` is a host-native u16 buffer (PSX bit layout,
   * host byte order). VRAM memory is PSX-LE bytes, so we byteswap on
   * store. Mask-bit checks also swap on read so the comparison against
   * host-native mask_and is correct. */
  if ((x + width) <= VRAM_WIDTH && (y + height) <= VRAM_HEIGHT && !params.IsMaskingEnabled())
  {
    const u16* src_ptr = static_cast<const u16*>(data);
    u16* dst_ptr = &m_vram_ptr[y * VRAM_WIDTH + x];
    u32* dst24_base = (g_pcsxr_true_color_active && g_psxVuw24)
                      ? &g_psxVuw24[y * VRAM_WIDTH + x] : 0;
    for (u32 yoffs = 0; yoffs < height; yoffs++)
    {
      /* Optimization (C): VRAMStoreLE -> sthbrx (1 instruccion). */
      for (u32 col = 0; col < width; ++col)
        VRAMStoreLE(&dst_ptr[col], src_ptr[col]);
      /* Shadow: bit-replicate 5-bit src to 8-bit. */
      if (dst24_base)
      {
        for (u32 col = 0; col < width; ++col)
        {
          const u16 s = src_ptr[col];
          const u32 r5 = (s >>  0) & 0x1F;
          const u32 g5 = (s >>  5) & 0x1F;
          const u32 b5 = (s >> 10) & 0x1F;
          dst24_base[col] = (((r5 << 3) | (r5 >> 2)) << 16)
                          | (((g5 << 3) | (g5 >> 2)) <<  8)
                          | (((b5 << 3) | (b5 >> 2)) <<  0);
        }
        dst24_base += VRAM_WIDTH;
      }
      src_ptr += width;
      dst_ptr += VRAM_WIDTH;
    }
  }
  else
  {
    const u16* src_ptr = static_cast<const u16*>(data);
    const u16 mask_and = params.GetMaskAND();
    const u16 mask_or = params.GetMaskOR();
    const bool tc = (g_pcsxr_true_color_active && g_psxVuw24);

    for (u32 row = 0; row < height;)
    {
      const u32 dst_row = (y + row++) & VRAM_HEIGHT_MASK;
      u16* dst_row_ptr = &m_vram_ptr[dst_row * VRAM_WIDTH];
      u32* dst_row24   = tc ? &g_psxVuw24[dst_row * VRAM_WIDTH] : 0;
      for (u32 col = 0; col < width;)
      {
        const u32 dst_col = (x + col++) & VRAM_WIDTH_MASK;
        u16* pixel_ptr = &dst_row_ptr[dst_col];
        if ((VRAMLoadLE(pixel_ptr) & mask_and) == 0)
        {
          /* Match original semantics: src_ptr advances ONLY when we
           * actually write.  Masked-out pixels reuse the same source. */
          const u16 src = *(src_ptr++);
          VRAMStoreLE(pixel_ptr, static_cast<u16>(src | mask_or));
          if (dst_row24)
          {
            const u32 r5 = (src >>  0) & 0x1F;
            const u32 g5 = (src >>  5) & 0x1F;
            const u32 b5 = (src >> 10) & 0x1F;
            dst_row24[dst_col] = (((r5 << 3) | (r5 >> 2)) << 16)
                               | (((g5 << 3) | (g5 >> 2)) <<  8)
                               | (((b5 << 3) | (b5 >> 2)) <<  0);
          }
        }
      }
    }
  }
}

void GPU_SW_Backend::CopyVRAM(u32 src_x, u32 src_y, u32 dst_x, u32 dst_y, u32 width, u32 height,
                              GPUBackendCommandParameters params)
{
  if ((src_x + width) > VRAM_WIDTH || (dst_x + width) > VRAM_WIDTH)
  {
    u32 remaining_rows = height;
    u32 current_src_y = src_y;
    u32 current_dst_y = dst_y;
    while (remaining_rows > 0)
    {
      const u32 rows_to_copy =
        (std::min<u32>)(remaining_rows, (std::min<u32>)(VRAM_HEIGHT - current_src_y, VRAM_HEIGHT - current_dst_y));

      u32 remaining_columns = width;
      u32 current_src_x = src_x;
      u32 current_dst_x = dst_x;
      while (remaining_columns > 0)
      {
        const u32 columns_to_copy =
          (std::min<u32>)(remaining_columns, (std::min<u32>)(VRAM_WIDTH - current_src_x, VRAM_WIDTH - current_dst_x));
        CopyVRAM(current_src_x, current_src_y, current_dst_x, current_dst_y, columns_to_copy, rows_to_copy, params);
        current_src_x = (current_src_x + columns_to_copy) & VRAM_WIDTH_MASK;
        current_dst_x = (current_dst_x + columns_to_copy) & VRAM_WIDTH_MASK;
        remaining_columns -= columns_to_copy;
      }

      current_src_y = (current_src_y + rows_to_copy) & VRAM_HEIGHT_MASK;
      current_dst_y = (current_dst_y + rows_to_copy) & VRAM_HEIGHT_MASK;
      remaining_rows -= rows_to_copy;
    }
    return;
  }

  const u16 mask_and = params.GetMaskAND();
  const u16 mask_or = params.GetMaskOR();
  const bool tc = (g_pcsxr_true_color_active && g_psxVuw24);

  /* Both src and dst rows live in psxVuw (PSX-LE bytes). The mask-bit
   * test must compare host-native values, so we swap both sides; the
   * OR with mask_or is applied in host-native space and swapped back
   * before the store.
   *
   * Para el shadow 24-bit: copiamos el slot correspondiente del src al
   * dst.  El shadow no tiene mask bit, asi que el mask_and check del
   * lado destino determina si copiamos o no (igual que el 5-bit). */
  if (src_x < dst_x || ((src_x + width - 1) & VRAM_WIDTH_MASK) < ((dst_x + width - 1) & VRAM_WIDTH_MASK))
  {
    for (u32 row = 0; row < height; row++)
    {
      const u32 src_y_eff = (src_y + row) & VRAM_HEIGHT_MASK;
      const u32 dst_y_eff = (dst_y + row) & VRAM_HEIGHT_MASK;
      const u16* src_row_ptr = &m_vram_ptr[src_y_eff * VRAM_WIDTH];
      u16* dst_row_ptr       = &m_vram_ptr[dst_y_eff * VRAM_WIDTH];
      const u32* src_row24   = tc ? &g_psxVuw24[src_y_eff * VRAM_WIDTH] : 0;
      u32*       dst_row24   = tc ? &g_psxVuw24[dst_y_eff * VRAM_WIDTH] : 0;

      /* Optimization (C): VRAMLoadLE/VRAMStoreLE -> lhbrx/sthbrx
       * (1 instruccion combinando load-or-store + byteswap). */
      for (s32 col = static_cast<s32>(width) - 1; col >= 0; col--)
      {
        const u32 sc = (src_x + static_cast<u32>(col)) & VRAM_WIDTH_MASK;
        const u32 dc = (dst_x + static_cast<u32>(col)) & VRAM_WIDTH_MASK;
        const u16 src_pixel = VRAMLoadLE(&src_row_ptr[sc]);
        u16* dst_pixel_ptr = &dst_row_ptr[dc];
        if ((VRAMLoadLE(dst_pixel_ptr) & mask_and) == 0)
        {
          VRAMStoreLE(dst_pixel_ptr, static_cast<u16>(src_pixel | mask_or));
          if (dst_row24) dst_row24[dc] = src_row24[sc];
        }
      }
    }
  }
  else
  {
    for (u32 row = 0; row < height; row++)
    {
      const u32 src_y_eff = (src_y + row) & VRAM_HEIGHT_MASK;
      const u32 dst_y_eff = (dst_y + row) & VRAM_HEIGHT_MASK;
      const u16* src_row_ptr = &m_vram_ptr[src_y_eff * VRAM_WIDTH];
      u16* dst_row_ptr       = &m_vram_ptr[dst_y_eff * VRAM_WIDTH];
      const u32* src_row24   = tc ? &g_psxVuw24[src_y_eff * VRAM_WIDTH] : 0;
      u32*       dst_row24   = tc ? &g_psxVuw24[dst_y_eff * VRAM_WIDTH] : 0;

      for (u32 col = 0; col < width; col++)
      {
        const u32 sc = (src_x + col) & VRAM_WIDTH_MASK;
        const u32 dc = (dst_x + col) & VRAM_WIDTH_MASK;
        const u16 src_pixel = VRAMLoadLE(&src_row_ptr[sc]);
        u16* dst_pixel_ptr = &dst_row_ptr[dc];
        if ((VRAMLoadLE(dst_pixel_ptr) & mask_and) == 0)
        {
          VRAMStoreLE(dst_pixel_ptr, static_cast<u16>(src_pixel | mask_or));
          if (dst_row24) dst_row24[dc] = src_row24[sc];
        }
      }
    }
  }
}

void GPU_SW_Backend::FlushRender() {}

/* ------------------------------------------------------------------
 * Function-pointer tables for primitive-specific template flavours.
 * Upstream used `static constexpr` — stripped on VS2010.
 * ---------------------------------------------------------------- */

GPU_SW_Backend::DrawLineFunction
GPU_SW_Backend::GetDrawLineFunction(bool shading_enable, bool transparency_enable, bool dithering_enable)
{
  static const DrawLineFunction funcs[2][2][2] = {
    {{&GPU_SW_Backend::DrawLineTemplated<false, false, false>, &GPU_SW_Backend::DrawLineTemplated<false, false, true>},
     {&GPU_SW_Backend::DrawLineTemplated<false, true,  false>, &GPU_SW_Backend::DrawLineTemplated<false, true,  true>}},
    {{&GPU_SW_Backend::DrawLineTemplated<true,  false, false>, &GPU_SW_Backend::DrawLineTemplated<true,  false, true>},
     {&GPU_SW_Backend::DrawLineTemplated<true,  true,  false>, &GPU_SW_Backend::DrawLineTemplated<true,  true,  true>}}
  };

  return funcs[static_cast<u8>(shading_enable)][static_cast<u8>(transparency_enable)][static_cast<u8>(dithering_enable)];
}

GPU_SW_Backend::DrawRectangleFunction
GPU_SW_Backend::GetDrawRectangleFunction(bool texture_enable, bool raw_texture_enable, bool transparency_enable)
{
  static const DrawRectangleFunction funcs[2][2][2] = {
    {{&GPU_SW_Backend::DrawRectangleTemplated<false, false, false>,
      &GPU_SW_Backend::DrawRectangleTemplated<false, false, true>},
     {&GPU_SW_Backend::DrawRectangleTemplated<false, false, false>,
      &GPU_SW_Backend::DrawRectangleTemplated<false, false, true>}},
    {{&GPU_SW_Backend::DrawRectangleTemplated<true,  false, false>,
      &GPU_SW_Backend::DrawRectangleTemplated<true,  false, true>},
     {&GPU_SW_Backend::DrawRectangleTemplated<true,  true,  false>,
      &GPU_SW_Backend::DrawRectangleTemplated<true,  true,  true>}}
  };

  return funcs[static_cast<u8>(texture_enable)][static_cast<u8>(raw_texture_enable)][static_cast<u8>(transparency_enable)];
}

GPU_SW_Backend::DrawTriangleFunction
GPU_SW_Backend::GetDrawTriangleFunction(bool shading_enable, bool texture_enable, bool raw_texture_enable,
                                        bool transparency_enable, bool dithering_enable)
{
  static const DrawTriangleFunction funcs[2][2][2][2][2] = {
    {{{{&GPU_SW_Backend::DrawTriangle<false, false, false, false, false>,
        &GPU_SW_Backend::DrawTriangle<false, false, false, false, true>},
       {&GPU_SW_Backend::DrawTriangle<false, false, false, true,  false>,
        &GPU_SW_Backend::DrawTriangle<false, false, false, true,  true>}},
      {{&GPU_SW_Backend::DrawTriangle<false, false, false, false, false>,
        &GPU_SW_Backend::DrawTriangle<false, false, false, false, false>},
       {&GPU_SW_Backend::DrawTriangle<false, false, false, true,  false>,
        &GPU_SW_Backend::DrawTriangle<false, false, false, true,  false>}}},
     {{{&GPU_SW_Backend::DrawTriangle<false, true,  false, false, false>,
        &GPU_SW_Backend::DrawTriangle<false, true,  false, false, true>},
       {&GPU_SW_Backend::DrawTriangle<false, true,  false, true,  false>,
        &GPU_SW_Backend::DrawTriangle<false, true,  false, true,  true>}},
      {{&GPU_SW_Backend::DrawTriangle<false, true,  true,  false, false>,
        &GPU_SW_Backend::DrawTriangle<false, true,  true,  false, false>},
       {&GPU_SW_Backend::DrawTriangle<false, true,  true,  true,  false>,
        &GPU_SW_Backend::DrawTriangle<false, true,  true,  true,  false>}}}},
    {{{{&GPU_SW_Backend::DrawTriangle<true,  false, false, false, false>,
        &GPU_SW_Backend::DrawTriangle<true,  false, false, false, true>},
       {&GPU_SW_Backend::DrawTriangle<true,  false, false, true,  false>,
        &GPU_SW_Backend::DrawTriangle<true,  false, false, true,  true>}},
      {{&GPU_SW_Backend::DrawTriangle<true,  false, false, false, false>,
        &GPU_SW_Backend::DrawTriangle<true,  false, false, false, false>},
       {&GPU_SW_Backend::DrawTriangle<true,  false, false, true,  false>,
        &GPU_SW_Backend::DrawTriangle<true,  false, false, true,  false>}}},
     {{{&GPU_SW_Backend::DrawTriangle<true,  true,  false, false, false>,
        &GPU_SW_Backend::DrawTriangle<true,  true,  false, false, true>},
       {&GPU_SW_Backend::DrawTriangle<true,  true,  false, true,  false>,
        &GPU_SW_Backend::DrawTriangle<true,  true,  false, true,  true>}},
      {{&GPU_SW_Backend::DrawTriangle<true,  true,  true,  false, false>,
        &GPU_SW_Backend::DrawTriangle<true,  true,  true,  false, false>},
       {&GPU_SW_Backend::DrawTriangle<true,  true,  true,  true,  false>,
        &GPU_SW_Backend::DrawTriangle<true,  true,  true,  true,  false>}}}}
  };

  return funcs[static_cast<u8>(shading_enable)][static_cast<u8>(texture_enable)]
              [static_cast<u8>(raw_texture_enable)][static_cast<u8>(transparency_enable)]
              [static_cast<u8>(dithering_enable)];
}
