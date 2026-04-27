/*
 * gpu_duck_gpu_types.h
 *
 * VS2010-compatible port of SwanStation's gpu_types.h.
 *
 * Changes from upstream:
 *   - `inline constexpr u32 X = ...;` (C++17 inline variables) converted
 *     to `static const u32 X = ...;` so each TU gets an internal-linkage
 *     copy (fine for integer constants used as array bounds / mask
 *     literals).
 *   - Binary literals `0b...` (C++14) rewritten as hex literals.
 *   - `static constexpr` array members (std::array, plain arrays)
 *     replaced with either `static` function-locals or `static const`.
 *   - Uses gpu_duck_bitfield.h / gpu_duck_rectangle.h / gpu_duck_types.h
 *     shims instead of the upstream common/ headers.
 */
#pragma once

#include "gpu_duck_compat.h"
#include "gpu_duck_bitfield.h"
#include "gpu_duck_rectangle.h"
#include "gpu_duck_types.h"

#include <array>

/* VRAM and texture-page constants.
 * Upstream used a single inline-constexpr chain; broken out here so
 * each declaration is plain pre-C++17 syntax. */
static const u32 VRAM_WIDTH           = 1024;
static const u32 VRAM_HEIGHT          = 512;
static const u32 VRAM_SIZE            = VRAM_WIDTH * VRAM_HEIGHT * sizeof(u16);
static const u32 VRAM_WIDTH_MASK      = VRAM_WIDTH - 1;
static const u32 VRAM_HEIGHT_MASK     = VRAM_HEIGHT - 1;
static const u32 TEXTURE_PAGE_WIDTH   = 256;
static const u32 TEXTURE_PAGE_HEIGHT  = 256;

/* In interlaced modes we can exceed the 512 height of VRAM, up to 576
 * in PAL games. */
static const u32 GPU_MAX_DISPLAY_WIDTH  = 720;
static const u32 GPU_MAX_DISPLAY_HEIGHT = 576;

static const u32 DITHER_MATRIX_SIZE = 4;

static const s32 MAX_PRIMITIVE_WIDTH  = 1024;
static const s32 MAX_PRIMITIVE_HEIGHT = 512;

/* VS2010 (MSC_VER 1600) does not support `enum class` with a fixed
 * underlying type — both features require >= 1700. We emulate scoped
 * enums by putting the enumerators inside a namespace; callers still
 * write `GPUPrimitive::Polygon` etc. Where the enum is used as a TYPE
 * (BitField template arg, struct member) callers must spell it as
 * `GPUPrimitive::Enum`. */
namespace GPUPrimitive
{
  enum Enum
  {
    Reserved  = 0,
    Polygon   = 1,
    Line      = 2,
    Rectangle = 3
  };
}

namespace GPUDrawRectangleSize
{
  enum Enum
  {
    Variable = 0,
    R1x1     = 1,
    R8x8     = 2,
    R16x16   = 3
  };
}

namespace GPUTextureMode
{
  enum Enum
  {
    Palette4Bit           = 0,
    Palette8Bit           = 1,
    Direct16Bit           = 2,
    Reserved_Direct16Bit  = 3,

    /* Not register values. Plain-enum bitwise via implicit int
     * conversion is well-defined at enum-initializer time. */
    RawTextureBit           = 4,
    RawPalette4Bit          = RawTextureBit | Palette4Bit,
    RawPalette8Bit          = RawTextureBit | Palette8Bit,
    RawDirect16Bit          = RawTextureBit | Direct16Bit,
    Reserved_RawDirect16Bit = RawTextureBit | Reserved_Direct16Bit,

    Disabled = 8 /* Not a register value. */
  };
}

/* Bitwise operators for GPUTextureMode::Enum. Plain enums implicitly
 * convert to int, but bitwise ops on them return int, not the enum.
 * We provide namespaced overloads so ported code can keep writing
 * `GPUTextureMode::RawTextureBit | GPUTextureMode::Palette4Bit` and get
 * a GPUTextureMode::Enum back. */
namespace GPUTextureMode
{
  ALWAYS_INLINE Enum operator|(Enum lhs, Enum rhs)
  { return static_cast<Enum>(static_cast<int>(lhs) | static_cast<int>(rhs)); }
  ALWAYS_INLINE Enum operator&(Enum lhs, Enum rhs)
  { return static_cast<Enum>(static_cast<int>(lhs) & static_cast<int>(rhs)); }
}

namespace GPUTransparencyMode
{
  enum Enum
  {
    HalfBackgroundPlusHalfForeground = 0,
    BackgroundPlusForeground         = 1,
    BackgroundMinusForeground        = 2,
    BackgroundPlusQuarterForeground  = 3,

    Disabled = 4 /* Not a register value. */
  };
}

namespace GPUInterlacedDisplayMode
{
  enum Enum
  {
    None,
    InterleavedFields,
    SeparateFields
  };
}

union GPURenderCommand
{
  u32 bits;

  BitField<u32, u32, 0, 24>                    color_for_first_vertex;
  BitField<u32, bool, 24, 1>                   raw_texture_enable;   /* not valid for lines */
  BitField<u32, bool, 25, 1>                   transparency_enable;
  BitField<u32, bool, 26, 1>                   texture_enable;
  BitField<u32, GPUDrawRectangleSize::Enum, 27, 2> rectangle_size;   /* only for rectangles */
  BitField<u32, bool, 27, 1>                       quad_polygon;     /* only for polygons */
  BitField<u32, bool, 27, 1>                       polyline;         /* only for lines */
  BitField<u32, bool, 28, 1>                       shading_enable;   /* 0 flat, 1 gouraud */
  BitField<u32, GPUPrimitive::Enum, 29, 21>        primitive;

  ALWAYS_INLINE bool IsTexturingEnabled() const
  {
    return (primitive != GPUPrimitive::Line) ? texture_enable : false;
  }

  ALWAYS_INLINE bool IsDitheringEnabled() const
  {
    switch (primitive)
    {
      case GPUPrimitive::Polygon:
        return shading_enable || (texture_enable && !raw_texture_enable);
      case GPUPrimitive::Line:
        return true;
      case GPUPrimitive::Rectangle:
      default:
        return false;
    }
  }
};

/* VRAM colour-format conversion helpers. Both stripped of constexpr
 * (shim-handled) and otherwise identical. */
ALWAYS_INLINE static u32 VRAMRGBA5551ToRGBA8888(u32 color)
{
  #define E5TO8(c) ((((c) * 527u) + 23u) >> 6)
  const u32 r = E5TO8(color & 31u);
  const u32 g = E5TO8((color >> 5) & 31u);
  const u32 b = E5TO8((color >> 10) & 31u);
  const u32 a = ((color >> 15) != 0) ? 255u : 0u;
  #undef E5TO8
  return ZeroExtend32(r) | (ZeroExtend32(g) << 8) | (ZeroExtend32(b) << 16) | (ZeroExtend32(a) << 24);
}

ALWAYS_INLINE static u16 VRAMRGBA8888ToRGBA5551(u32 color)
{
  const u32 r = (color & 0xFFu) >> 3;
  const u32 g = ((color >> 8) & 0xFFu) >> 3;
  const u32 b = ((color >> 16) & 0xFFu) >> 3;
  const u32 a = ((color >> 24) & 0x01u);
  return Truncate16(r | (g << 5) | (b << 10) | (a << 15));
}

union GPUVertexPosition
{
  u32 bits;
  BitField<u32, s32, 0,  11> x;
  BitField<u32, s32, 16, 11> y;
};

/* Sprites/rectangles should be clipped to 12 bits before drawing. */
static inline s32 TruncateGPUVertexPosition(s32 x)
{
  return SignExtendN<11, s32>(x);
}

/* GP0(E1h) / polygon-texpage register. Masks are hex equivalents of
 * the upstream 0b... binary literals. */
union GPUDrawModeReg
{
  /* 0b1111111111111  == 0x1FFF (13 bits) */
  static const u16 MASK                 = 0x1FFFu;
  /* 0b0000000000011111 == 0x001F */
  static const u16 TEXTURE_PAGE_MASK    = 0x001Fu;
  /* 0b0000100111111111 == 0x09FF */
  static const u16 POLYGON_TEXPAGE_MASK = 0x09FFu;
  /* 0b11111111111 == 0x07FF (11 bits) */
  static const u32 GPUSTAT_MASK         = 0x07FFu;

  u16 bits;

  BitField<u16, u8, 0, 4>                   texture_page_x_base;
  BitField<u16, u8, 4, 1>                   texture_page_y_base;
  BitField<u16, GPUTransparencyMode::Enum, 5, 2> transparency_mode;
  BitField<u16, GPUTextureMode::Enum, 7, 2>      texture_mode;
  BitField<u16, bool, 9,  1>                dither_enable;
  BitField<u16, bool, 10, 1>                draw_to_displayed_field;
  BitField<u16, bool, 11, 1>                texture_disable;
  BitField<u16, bool, 12, 1>                texture_x_flip;
  BitField<u16, bool, 13, 1>                texture_y_flip;

  ALWAYS_INLINE u16 GetTexturePageBaseX() const
  {
    return static_cast<u16>(ZeroExtend16(texture_page_x_base.GetValue()) * 64u);
  }
  ALWAYS_INLINE u16 GetTexturePageBaseY() const
  {
    return static_cast<u16>(ZeroExtend16(texture_page_y_base.GetValue()) * 256u);
  }

  ALWAYS_INLINE bool IsUsingPalette() const { return (bits & (2 << 7)) == 0; }

  /* Returns a rectangle comprising the texture-page area.
   * Upstream used `static constexpr std::array<u32,4>` here; we use a
   * regular `static const u32[]` so VS2010 accepts it without the
   * aggregate-init-in-constexpr pitfalls. */
  ALWAYS_INLINE_RELEASE Common::Rectangle<u32> GetTexturePageRectangle() const
  {
    static const u32 texture_page_widths[4] = {
      TEXTURE_PAGE_WIDTH / 4,
      TEXTURE_PAGE_WIDTH / 2,
      TEXTURE_PAGE_WIDTH,
      TEXTURE_PAGE_WIDTH
    };
    return Common::Rectangle<u32>::FromExtents(
      GetTexturePageBaseX(),
      GetTexturePageBaseY(),
      texture_page_widths[static_cast<u8>(texture_mode.GetValue())],
      TEXTURE_PAGE_HEIGHT);
  }
};

union GPUTexturePaletteReg
{
  /* 0b0111111111111111 == 0x7FFF */
  static const u16 MASK = 0x7FFFu;

  u16 bits;

  BitField<u16, u16, 0, 6> x;
  BitField<u16, u16, 6, 9> y;

  ALWAYS_INLINE u32 GetXBase() const { return static_cast<u32>(static_cast<u16>(x)) * 16u; }
  ALWAYS_INLINE u32 GetYBase() const { return static_cast<u32>(static_cast<u16>(y)); }
};

struct GPUTextureWindow
{
  u8 and_x;
  u8 and_y;
  u8 or_x;
  u8 or_y;
};

/* 4x4 dither matrix. Declared in header with `static` so every
 * including TU gets its own internal-linkage copy (no ODR issue). */
static const s32 DITHER_MATRIX[DITHER_MATRIX_SIZE][DITHER_MATRIX_SIZE] = {
  {-4, +0, -3, +1},
  {+2, -2, +3, -1},
  {-3, +1, -4, +0},
  {+3, -1, +2, -2}
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200) /* zero-sized array in struct/union (flexible array member) */
#endif

namespace GPUBackendCommandType
{
  enum Enum
  {
    Wraparound,
    Sync,
    FillVRAM,
    UpdateVRAM,
    CopyVRAM,
    SetDrawingArea,
    DrawPolygon,
    DrawRectangle,
    DrawLine
  };
}

union GPUBackendCommandParameters
{
  u8 bits;

  BitField<u8, bool, 0, 1>  interlaced_rendering;
  BitField<u8, u8,   1, 1>  active_line_lsb;
  BitField<u8, bool, 2, 1>  set_mask_while_drawing;
  BitField<u8, bool, 3, 1>  check_mask_before_draw;

  ALWAYS_INLINE bool IsMaskingEnabled() const { return (bits & 12u) != 0u; }

  u16 GetMaskAND() const { return Truncate16((bits << 12) & 0x8000); }
  u16 GetMaskOR()  const { return Truncate16((bits << 13) & 0x8000); }
};

struct GPUBackendCommand
{
  u32                         size;
  GPUBackendCommandType::Enum type;
  GPUBackendCommandParameters params;
};

struct GPUBackendSyncCommand : public GPUBackendCommand
{
  bool allow_sleep;
};

struct GPUBackendFillVRAMCommand : public GPUBackendCommand
{
  u16 x;
  u16 y;
  u16 width;
  u16 height;
  u32 color;
};

struct GPUBackendUpdateVRAMCommand : public GPUBackendCommand
{
  u16 x;
  u16 y;
  u16 width;
  u16 height;
  u16 data[0]; /* flexible array member (MSVC extension, C4200 above) */
};

struct GPUBackendCopyVRAMCommand : public GPUBackendCommand
{
  u16 src_x;
  u16 src_y;
  u16 dst_x;
  u16 dst_y;
  u16 width;
  u16 height;
};

struct GPUBackendSetDrawingAreaCommand : public GPUBackendCommand
{
  Common::Rectangle<u32> new_area;
};

struct GPUBackendDrawCommand : public GPUBackendCommand
{
  GPUDrawModeReg       draw_mode;
  GPURenderCommand     rc;
  GPUTexturePaletteReg palette;
  GPUTextureWindow     window;

  ALWAYS_INLINE bool IsDitheringEnabled() const
  {
    return rc.IsDitheringEnabled() && draw_mode.dither_enable;
  }
};

struct GPUBackendDrawPolygonCommand : public GPUBackendDrawCommand
{
  u16 num_vertices;

  struct Vertex
  {
    s32 x, y;
    /* PSX GP0 colors are 24-bit BBGGRR packed into a u32 with R in the
     * low byte (host-native). On LE, the in-memory byte sequence is
     * [R, G, B, A], so `u8 r, g, b, a` aligns naturally. On BE, the
     * same host-native u32 is stored as [A, B, G, R], so we reverse
     * the struct field order to keep each named field mapped to the
     * same logical channel. Without this, the rasterizer's interpolator
     * reads red as 0, green as blue, and blue as green, producing the
     * green tint / shifted-channel artefacts we saw on Xbox 360.
     *
     * Texcoord gets the same treatment: PSX texcoord is a u16 with U
     * in the low byte and V in the high byte. On LE, bytes are [U, V];
     * on BE, bytes are [V, U], so we reverse the struct fields. */
#if DUCK_BIG_ENDIAN
    union
    {
      struct
      {
        u8 a, b, g, r;
      };
      u32 color;
    };
    union
    {
      struct
      {
        u8 v, u;
      };
      u16 texcoord;
    };
#else
    union
    {
      struct
      {
        u8 r, g, b, a;
      };
      u32 color;
    };
    union
    {
      struct
      {
        u8 u, v;
      };
      u16 texcoord;
    };
#endif

    ALWAYS_INLINE void Set(s32 x_, s32 y_, u32 color_, u16 texcoord_)
    {
      x = x_;
      y = y_;
      color = color_;
      texcoord = texcoord_;
    }
  };

  Vertex vertices[0];
};

struct GPUBackendDrawRectangleCommand : public GPUBackendDrawCommand
{
  s32 x, y;
  u16 width, height;
  u16 texcoord;
  u32 color;
};

struct GPUBackendDrawLineCommand : public GPUBackendDrawCommand
{
  u16 num_vertices;

  struct Vertex
  {
    s32 x, y;
    /* Same BE reversal as GPUBackendDrawPolygonCommand::Vertex above —
     * see that comment for rationale. */
#if DUCK_BIG_ENDIAN
    union
    {
      struct
      {
        u8 a, b, g, r;
      };
      u32 color;
    };
#else
    union
    {
      struct
      {
        u8 r, g, b, a;
      };
      u32 color;
    };
#endif

    ALWAYS_INLINE void Set(s32 x_, s32 y_, u32 color_)
    {
      x = x_;
      y = y_;
      color = color_;
    }
  };

  Vertex vertices[0];
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif
