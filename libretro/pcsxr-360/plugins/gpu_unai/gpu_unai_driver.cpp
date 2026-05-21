/*
 * gpu_unai_driver.cpp
 *
 * Bridge between pcsxr-360's GP0 dispatch surface and the gpu_unai
 * software rasteriser ported from PCSX-ReARMed.
 *
 * Modelled on gpu_duck_driver.cpp.  xbox_soft/gpu.c routes its
 * primFunc dispatch through `unai_primTable` when the libretro option
 * `pcsxr360_gpu_renderer = gpu_unai` is selected.  Each entry in the
 * table receives the fully-accumulated raw GP0 packet (already byte-
 * size-correct, just like primTableJ) and translates it into the
 * gpu_unai PacketBuffer + dispatch.
 *
 * VRAM is shared via pointer with psxVuw, so no copy on the hot path.
 */

#include "gpu_unai_driver.h"

/* Order matters here: gpu_unai.h forward-declares the gpu_unai struct;
 * gpu_fixedpoint.h provides the FIXED_BITS macros used by the rasters;
 * gpu_inner.h pulls in the templated drivers (and via it,
 * gpu_inner_blend / quantization / light); then the gpu_raster_*.h
 * headers depend on all of the above. */
#include "gpu_unai.h"
#include "gpu_fixedpoint.h"
#include "gpu_inner.h"
#include "gpu_raster_image.h"
#include "gpu_raster_line.h"
#include "gpu_raster_polygon.h"
#include "gpu_raster_sprite.h"
#include "gpu_command.h"

#include <string.h>
#include <stdint.h>

/* ==================================================================
 * Global state.
 *
 * Upstream gpu_unai.h declares `static __attribute__((aligned(32)))
 * gpu_unai_t gpu_unai;` and `gpu_unai_config_t gpu_unai_config_ext;`
 * directly inside the header.  In our port the header has them as
 * `extern` and the actual definitions live here, so multiple TUs
 * referencing them link cleanly.
 * ================================================================ */

/* C linkage for both globals so the symbol names match the
 * `extern "C"` declarations in libretro_core.cpp / xbox_soft.
 * `extern "C" T name;` ALONE is a *declaration*, not a definition —
 * we need the brace form below to actually emit the variable in this
 * translation unit. */
extern "C" {
    __declspec(align(32)) gpu_unai_t gpu_unai;
    gpu_unai_config_t gpu_unai_config_ext;
}

/* Renderer-active flag.  xbox_soft/gpu.c reads this each frame to
 * decide whether to dispatch through unai_primTable.  Default 0 so
 * a build that doesn't surface the option still uses PEOPS. */
extern "C" int unai_gpu_enabled = 0;

/* ==================================================================
 * Stubs for gpulib helpers we don't have on Xbox 360.
 *
 * Upstream `gpulib_if.cpp` calls into gpulib for two things we mirror
 * here trivially:
 *   - `cmd_lengths[256]`: GP0 packet length lookup.  pcsxr-360 already
 *     does its own packet-length lookup in primTableCX (xbox_soft) so
 *     by the time our handlers fire we already have the full packet.
 *     We provide a copy of cmd_lengths here only for completeness; the
 *     dispatch surface doesn't actually consult it.
 *   - `prim_try_simplify_quad_t/_gt`: quad-to-sprite simplification
 *     (textured rectangle masquerading as a textured quad).  Skipped
 *     for now — we always go through gpuDrawPolyFT/gpuDrawPolyGT.
 *     The visual difference is negligible; the cycle savings vanish
 *     because pcsxr-360's GPU helper thread runs in parallel anyway.
 * ================================================================ */

extern "C" const unsigned char cmd_lengths[256] = {
    0,  0,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    4,  4,  4,  4,  7,  7,  7,  7,  5,  5,  5,  5,  9,  9,  9,  9,
    6,  6,  6,  6, 10, 10, 10, 10,  8,  8,  8,  8, 12, 12, 12, 12,
    3,  3,  3,  3,  0,  0,  0,  0,  6,  6,  6,  6,  0,  0,  0,  0,
    4,  4,  4,  4,  0,  0,  0,  0,  8,  8,  8,  8,  0,  0,  0,  0,
    3,  3,  3,  3,  4,  4,  4,  4,  3,  3,  3,  3,  4,  4,  4,  4,
    3,  3,  3,  3,  4,  4,  4,  4,  3,  3,  3,  3,  4,  4,  4,  4,
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    0,  1,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

/* No-op simplifications.  Returning 0 instructs the dispatcher to
 * use the full polygon path.  See README in upstream for the full
 * algorithm if we want to revisit this for 1-2% extra speed. */
extern "C" int prim_try_simplify_quad_t (void* /*simplified*/, const void* /*prim*/) { return 0; }
extern "C" int prim_try_simplify_quad_gt(void* /*simplified*/, const void* /*prim*/) { return 0; }

/* ==================================================================
 * Driver lifecycle.
 * ================================================================ */

extern "C" int unai_init(unsigned short* psx_vram)
{
    /* Zero the entire state struct, then re-initialise the parts that
     * need non-zero defaults.  Mirrors `renderer_init` from upstream's
     * gpulib_if.cpp. */
    memset((void*)&gpu_unai, 0, sizeof(gpu_unai));
    gpu_unai.vram = (le16_t*)psx_vram;

    /* Texture window defaults: full 256x256, no offset. */
    gpu_unai.TextureWindow[0] = 0;
    gpu_unai.TextureWindow[1] = 0;
    gpu_unai.TextureWindow[2] = 255;
    gpu_unai.TextureWindow[3] = 255;
    gpu_unai.inn.mask_v00u =
        ((u32)gpu_unai.TextureWindow[3] << 24) | gpu_unai.TextureWindow[2];

    /* Drawing area defaults: full VRAM. */
    gpu_unai.DrawingArea[0] = 0;
    gpu_unai.DrawingArea[1] = 0;
    gpu_unai.DrawingArea[2] = 1024;
    gpu_unai.DrawingArea[3] = 512;

    /* Pull config in.  Frontend should populate gpu_unai_config_ext
     * before calling this, but if not, lighting/blending/dithering
     * default to 0 (matches the PCSX-ReARMed default for slow
     * devices). */
    gpu_unai.config = gpu_unai_config_ext;
    gpu_unai.inn.ilace_mask = gpu_unai.config.ilace_force;

    /* Build the LUTs.  These are global one-shot tables, not per-frame
     * state, so the cost is hidden behind init. */
    SetupLightLUT();
    SetupDitheringConstants();

    return 0;
}

extern "C" void unai_shutdown(void)
{
    /* Nothing to free — gpu_unai is a static struct, no allocations. */
}

extern "C" void unai_reset(void)
{
    /* Re-init with the same VRAM pointer.  Drops any draw mode /
     * texture page state — frontend is expected to send the GP1 reset
     * sequence afterwards which will repopulate. */
    unsigned short* vram = (unsigned short*)gpu_unai.vram;
    unai_init(vram);
}

extern "C" void unai_apply_config(void)
{
    /* Hot-reload of frontend toggles.  Called when libretro signals
     * variable-update.  Just refresh from gpu_unai_config_ext — the
     * inline helpers (LightingEnabled / BlendingEnabled / etc.) read
     * gpu_unai.config every frame, so no further work needed. */
    gpu_unai.config = gpu_unai_config_ext;
    gpu_unai.inn.ilace_mask = gpu_unai.config.ilace_force;
}

/* ==================================================================
 * Helpers used by the per-opcode handlers below.
 * ================================================================ */

/* Copy `len` 32-bit words from baseAddr into PacketBuffer.U4[].
 * baseAddr is in PSX-LE byte order (xbox_soft accumulator copies the
 * PSX bytes as-is).  PacketBuffer.U4 expects each entry to be a
 * `le32_t` whose raw payload is also PSX-LE (the LExRead macros
 * byteswap on access).  So this is a straight memcpy. */
static __forceinline void load_packet_words(const unsigned char* baseAddr, int len)
{
    if (len > 16) len = 16;   /* PacketBuffer is 16 words */
    memcpy(gpu_unai.PacketBuffer.U4, baseAddr, len * 4);
}

/* GP0 register helper used by 0xE1..0xE6 handlers.  Mirrors
 * `gpuGP0Cmd_0xEx` from gpulib_if.cpp.  The cmd_word is in PSX byte
 * order (host-LE-equivalent after byteswap on BE). */
static void unai_gp0_setting(u32 cmd_word)
{
    u8 num = (cmd_word >> 24) & 7;
    switch (num) {
    case 1: {
        /* GP0(E1h) — Draw Mode setting (Texpage) */
        u32 cur_texpage = gpu_unai.GPU_GP1 & 0x7FF;
        u32 new_texpage = cmd_word & 0x7FF;
        if (cur_texpage != new_texpage) {
            gpu_unai.GPU_GP1 = (gpu_unai.GPU_GP1 & ~0x7FF) | new_texpage;
            gpuSetTexture((u16)gpu_unai.GPU_GP1);
        }
    } break;

    case 2: {
        /* GP0(E2h) — Texture Window setting */
        if (cmd_word != gpu_unai.TextureWindowCur) {
            static const u8 TextureMask[32] = {
                255, 7, 15, 7, 31, 7, 15, 7, 63, 7, 15, 7, 31, 7, 15, 7,
                127, 7, 15, 7, 31, 7, 15, 7, 63, 7, 15, 7, 31, 7, 15, 7
            };
            gpu_unai.TextureWindowCur = cmd_word;
            gpu_unai.TextureWindow[0] = ((cmd_word >> 10) & 0x1F) << 3;
            gpu_unai.TextureWindow[1] = ((cmd_word >> 15) & 0x1F) << 3;
            gpu_unai.TextureWindow[2] = TextureMask[(cmd_word >> 0) & 0x1F];
            gpu_unai.TextureWindow[3] = TextureMask[(cmd_word >> 5) & 0x1F];
            gpu_unai.TextureWindow[0] &= ~gpu_unai.TextureWindow[2];
            gpu_unai.TextureWindow[1] &= ~gpu_unai.TextureWindow[3];

            gpu_unai.inn.mask_v00u =
                ((u32)gpu_unai.TextureWindow[3] << 24) | gpu_unai.TextureWindow[2];

            gpuSetTexture((u16)gpu_unai.GPU_GP1);
        }
    } break;

    case 3: {
        /* GP0(E3h) — Drawing Area top-left */
        gpu_unai.DrawingArea[0] = (u16)( cmd_word        & 0x3FF);
        gpu_unai.DrawingArea[1] = (u16)((cmd_word >> 10) & 0x3FF);
    } break;

    case 4: {
        /* GP0(E4h) — Drawing Area bottom-right */
        gpu_unai.DrawingArea[2] = (u16)(( cmd_word        & 0x3FF) + 1);
        gpu_unai.DrawingArea[3] = (u16)(((cmd_word >> 10) & 0x3FF) + 1);
    } break;

    case 5: {
        /* GP0(E5h) — Drawing Offset */
        gpu_unai.DrawingOffset[0] = (s16)GPU_EXPANDSIGN(cmd_word);
        gpu_unai.DrawingOffset[1] = (s16)GPU_EXPANDSIGN(cmd_word >> 11);
    } break;

    case 6: {
        /* GP0(E6h) — Mask Bit Setting */
        gpu_unai.Masking  = (u8)((cmd_word & 0x2) << 1);
        gpu_unai.PixelMSB = (u16)((cmd_word & 0x1) << 8);
    } break;
    }
}

/* Convenience: read one 32-bit PSX-LE word from baseAddr. */
static __forceinline u32 raw_word(const unsigned char* baseAddr, int idx)
{
    /* Stored LE on disk → host-LE equivalent.  On BE host we byteswap
     * via le32_to_u32 / le32_raw to get the host-native value. */
    le32_t le;
    memcpy(&le, baseAddr + (idx * 4), 4);
    return le32_to_u32(le);
}

/* Quick check: does an RGB triplet need lighting?  Strip lower 3 bits
 * and see if any color differs from neutral grey 0x80.  Mirrors
 * upstream `need_lighting`. */
static __forceinline bool need_lighting(u32 rgb_raw_le)
{
    return (rgb_raw_le & HTOLE32(0xF8F8F8)) != HTOLE32(0x808080);
}

/* Convenience macro for the opcode-byte extraction used by the
 * driver_idx calculations below. */
#define PRIM(baseAddr) ((unsigned)(baseAddr)[3])

#define Blending      (((PRIM(baseAddr)&0x2) && BlendingEnabled()) ? (PRIM(baseAddr)&0x2) : 0)
#define Blending_Mode (((PRIM(baseAddr)&0x2) && BlendingEnabled()) ? gpu_unai.BLEND_MODE : 0)
#define Lighting      (((~PRIM(baseAddr))&0x1) && LightingEnabled())
#define Dithering     (((((~PRIM(baseAddr))&0x1) || (PRIM(baseAddr)&0x10)) && DitheringEnabled()) ? \
                       (ForcedDitheringEnabled() ? (1<<9) : (gpu_unai.GPU_GP1 & (1 << 9))) : 0)

/* ==================================================================
 * Per-opcode handlers.
 * ================================================================ */

static void unai_primNI(unsigned char* /*baseAddr*/) { /* not implemented */ }

static void unai_primBlkFill(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 3);
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    gpuClearImage(packet);
}

/* LoadImage / StoreImage / MoveImage are delegated to xbox_soft.
 * The pixel payload doesn't ride in the GP0 packet — it streams via
 * PEOPS_GPUwriteDataMem into psxVuw, which IS gpu_unai.vram, so the
 * existing xbox_soft path (primLoadImage / primStoreImage / primMove
 * Image) does the right thing. */
extern "C" void primLoadImage(unsigned char* baseAddr);
extern "C" void primStoreImage(unsigned char* baseAddr);
extern "C" void primMoveImage(unsigned char* baseAddr);

/* Polygon handlers ------------------------------------------------- */

static void unai_primPolyF3(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 4);
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    PP driver = gpuPolySpanDrivers[
        Blending_Mode | gpu_unai.Masking | Blending | gpu_unai.PixelMSB
    ];
    gpuDrawPolyF(packet, driver, false);
}

static void unai_primPolyF4(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 5);
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    PP driver = gpuPolySpanDrivers[
        Blending_Mode | gpu_unai.Masking | Blending | gpu_unai.PixelMSB
    ];
    gpuDrawPolyF(packet, driver, true);
}

static void unai_primPolyFT3(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 7);
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    gpuSetCLUT   ((u16)(le32_to_u32(gpu_unai.PacketBuffer.U4[2]) >> 16));
    gpuSetTexture((u16)(le32_to_u32(gpu_unai.PacketBuffer.U4[4]) >> 16));

    u32 driver_idx =
        Dithering |
        Blending_Mode | gpu_unai.TEXT_MODE |
        gpu_unai.Masking | Blending | gpu_unai.PixelMSB;

    if (!FastLightingEnabled()) {
        driver_idx |= Lighting;
    } else {
        if (!((gpu_unai.PacketBuffer.U1[0]>0x5F) && (gpu_unai.PacketBuffer.U1[1]>0x5F) && (gpu_unai.PacketBuffer.U1[2]>0x5F)))
            driver_idx |= Lighting;
    }

    PP driver = gpuPolySpanDrivers[driver_idx];
    gpuDrawPolyFT(packet, driver, false);
}

static void unai_primPolyFT4(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 9);
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    gpuSetCLUT   ((u16)(le32_to_u32(gpu_unai.PacketBuffer.U4[2]) >> 16));
    gpuSetTexture((u16)(le32_to_u32(gpu_unai.PacketBuffer.U4[4]) >> 16));

    u32 driver_idx =
        Dithering |
        Blending_Mode | gpu_unai.TEXT_MODE |
        gpu_unai.Masking | Blending | gpu_unai.PixelMSB;

    if (!FastLightingEnabled()) {
        driver_idx |= Lighting;
    } else {
        if (!((gpu_unai.PacketBuffer.U1[0]>0x5F) && (gpu_unai.PacketBuffer.U1[1]>0x5F) && (gpu_unai.PacketBuffer.U1[2]>0x5F)))
            driver_idx |= Lighting;
    }

    PP driver = gpuPolySpanDrivers[driver_idx];
    gpuDrawPolyFT(packet, driver, true);
}

static void unai_primPolyG3(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 6);
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    u32 dithering = Dithering;
    u8 gouraud = 129;
    if (!dithering) {
        u32 xor_ = 0, rgb0 = le32_raw(gpu_unai.PacketBuffer.U4[0]);
        for (int i = 1; i < 3; i++)
            xor_ |= rgb0 ^ le32_raw(gpu_unai.PacketBuffer.U4[i * 2]);
        if ((xor_ & HTOLE32(0xf8f8f8)) == 0)
            gouraud = 0;
    }
    PP driver = gpuPolySpanDrivers[
        dithering | Blending_Mode |
        gpu_unai.Masking | Blending | gouraud | gpu_unai.PixelMSB
    ];
    if (gouraud)
        gpuDrawPolyG(packet, driver, false);
    else
        gpuDrawPolyF(packet, driver, false, POLYTYPE_G);
}

static void unai_primPolyG4(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 8);
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    u32 dithering = Dithering;
    u8 gouraud = 129;
    if (!dithering) {
        u32 xor_ = 0, rgb0 = le32_raw(gpu_unai.PacketBuffer.U4[0]);
        for (int i = 1; i < 4; i++)
            xor_ |= rgb0 ^ le32_raw(gpu_unai.PacketBuffer.U4[i * 2]);
        if ((xor_ & HTOLE32(0xf8f8f8)) == 0)
            gouraud = 0;
    }
    PP driver = gpuPolySpanDrivers[
        dithering | Blending_Mode |
        gpu_unai.Masking | Blending | gouraud | gpu_unai.PixelMSB
    ];
    if (gouraud)
        gpuDrawPolyG(packet, driver, true);
    else
        gpuDrawPolyF(packet, driver, true, POLYTYPE_G);
}

static void unai_primPolyGT3(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 9);
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    gpuSetCLUT   ((u16)(le32_to_u32(gpu_unai.PacketBuffer.U4[2]) >> 16));
    gpuSetTexture((u16)(le32_to_u32(gpu_unai.PacketBuffer.U4[5]) >> 16));
    u32 dithering = Dithering;
    u8 lighting = Lighting;
    u8 gouraud = lighting ? (1<<7) : 0;
    if (lighting) {
        u32 xor_ = 0, rgb0 = le32_raw(gpu_unai.PacketBuffer.U4[0]);
        for (int i = 1; i < 3; i++)
            xor_ |= rgb0 ^ le32_raw(gpu_unai.PacketBuffer.U4[i * 3]);
        if ((xor_ & HTOLE32(0xf8f8f8)) == 0) {
            gouraud = 0;
            /* Si los 3 vertices tienen el mismo color Y la iluminacion es
             * neutra (0x808080), saltamos lighting Y dithering. El dithering
             * sobre textura con luz neutra es practicamente invisible (anade
             * +/-1 LSB de error en pares aislados, ya enmascarado por la
             * cuantizacion de la textura) y nos ahorra entrar al inner-loop
             * caro con clamp_c por canal. Silent Hill usa este tipo de polys
             * masivamente para la niebla; sin este shortcut se cuelga el
             * juego porque el frame timing se desincroniza. */
            if (!need_lighting(rgb0)) {
                lighting = 0;
                dithering = 0;
            }
        }
    }
    PP driver = gpuPolySpanDrivers[
        dithering | Blending_Mode | gpu_unai.TEXT_MODE |
        gpu_unai.Masking | Blending | gouraud | lighting | gpu_unai.PixelMSB
    ];
    if (gouraud)
        gpuDrawPolyGT(packet, driver, false);
    else
        gpuDrawPolyFT(packet, driver, false, POLYTYPE_GT);
}

static void unai_primPolyGT4(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 12);
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    gpuSetCLUT   ((u16)(le32_to_u32(gpu_unai.PacketBuffer.U4[2]) >> 16));
    gpuSetTexture((u16)(le32_to_u32(gpu_unai.PacketBuffer.U4[5]) >> 16));
    u32 dithering = Dithering;
    u8 lighting = Lighting;
    u8 gouraud = lighting ? (1<<7) : 0;
    if (lighting) {
        u32 xor_ = 0, rgb0 = le32_raw(gpu_unai.PacketBuffer.U4[0]);
        for (int i = 1; i < 4; i++)
            xor_ |= rgb0 ^ le32_raw(gpu_unai.PacketBuffer.U4[i * 3]);
        if ((xor_ & HTOLE32(0xf8f8f8)) == 0) {
            gouraud = 0;
            /* Mismo shortcut que en PolyGT3: textura con luz neutra salta
             * tambien el dither. Critico para Silent Hill. */
            if (!need_lighting(rgb0)) {
                lighting = 0;
                dithering = 0;
            }
        }
    }
    PP driver = gpuPolySpanDrivers[
        dithering | Blending_Mode | gpu_unai.TEXT_MODE |
        gpu_unai.Masking | Blending | gouraud | lighting | gpu_unai.PixelMSB
    ];
    if (gouraud)
        gpuDrawPolyGT(packet, driver, true);
    else
        gpuDrawPolyFT(packet, driver, true, POLYTYPE_GT);
}

/* Line handlers --------------------------------------------------- */

static void unai_primLineF2(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 3);
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    u32 driver_idx = (Blending_Mode | gpu_unai.Masking | Blending | (gpu_unai.PixelMSB>>3)) >> 1;
    PSD driver = gpuPixelSpanDrivers[driver_idx];
    gpuDrawLineF(packet, driver);
}

static void unai_primLineG2(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 4);
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    u32 driver_idx = (Blending_Mode | gpu_unai.Masking | Blending | (gpu_unai.PixelMSB>>3)) >> 1;
    driver_idx |= (1 << 5);  /* Gouraud-shaded PixelSpanDriver */
    PSD driver = gpuPixelSpanDrivers[driver_idx];
    gpuDrawLineG(packet, driver);
}

/* Polylines.  In pcsxr-360 / xbox_soft these come pre-segmented:
 * primLineFEx / primLineGEx walk the chain and call here for each
 * segment.  The opcode dispatch table maps 0x48..0x4F and 0x58..0x5F
 * to the same handler — the gpuDrawLine* functions read up to 4 words
 * from PacketBuffer regardless. */

/* Sprite/rectangle handlers --------------------------------------- */

static void unai_textured_sprite(unsigned char* baseAddr)
{
    /* Caller has already filled PacketBuffer.U4[0..3].  Mirrors the
     * `textured_sprite` helper in upstream gpulib_if.cpp. */
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    gpuSetCLUT((u16)(le32_to_u32(gpu_unai.PacketBuffer.U4[2]) >> 16));
    u32 driver_idx = Blending_Mode | gpu_unai.TEXT_MODE | gpu_unai.Masking | Blending | (gpu_unai.PixelMSB>>1);
    if (need_lighting(le32_raw(gpu_unai.PacketBuffer.U4[0])))
        driver_idx |= Lighting;
    PS driver = gpuSpriteDrivers[driver_idx];
    s32 w = 0, h = 0;
    gpuDrawS(packet, driver, &w, &h);
    (void)w; (void)h;   /* cycle counting handled by pcsxr-360 elsewhere */
}

static void unai_primTileVar(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 3);
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    PT driver = gpuTileDrivers[(Blending_Mode | gpu_unai.Masking | Blending | (gpu_unai.PixelMSB>>3)) >> 1];
    s32 w = 0, h = 0;
    gpuDrawT(packet, driver, &w, &h);
    (void)w; (void)h;
}

static void unai_primTile1(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 2);
    gpu_unai.PacketBuffer.U4[2] = u32_to_le32(0x00010001);
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    PT driver = gpuTileDrivers[(Blending_Mode | gpu_unai.Masking | Blending | (gpu_unai.PixelMSB>>3)) >> 1];
    s32 w = 0, h = 0;
    gpuDrawT(packet, driver, &w, &h);
    (void)w; (void)h;
}

static void unai_primTile8(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 2);
    gpu_unai.PacketBuffer.U4[2] = u32_to_le32(0x00080008);
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    PT driver = gpuTileDrivers[(Blending_Mode | gpu_unai.Masking | Blending | (gpu_unai.PixelMSB>>3)) >> 1];
    s32 w = 0, h = 0;
    gpuDrawT(packet, driver, &w, &h);
    (void)w; (void)h;
}

static void unai_primTile16(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 2);
    gpu_unai.PacketBuffer.U4[2] = u32_to_le32(0x00100010);
    PtrUnion packet; packet.ptr = (void*)&gpu_unai.PacketBuffer;
    PT driver = gpuTileDrivers[(Blending_Mode | gpu_unai.Masking | Blending | (gpu_unai.PixelMSB>>3)) >> 1];
    s32 w = 0, h = 0;
    gpuDrawT(packet, driver, &w, &h);
    (void)w; (void)h;
}

static void unai_primSpriteVar(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 4);
    unai_textured_sprite(baseAddr);
}

static void unai_primSprite8(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 3);
    gpu_unai.PacketBuffer.U4[3] = u32_to_le32(0x00080008);
    unai_textured_sprite(baseAddr);
}

static void unai_primSprite16(unsigned char* baseAddr)
{
    load_packet_words(baseAddr, 3);
    gpu_unai.PacketBuffer.U4[3] = u32_to_le32(0x00100010);
    unai_textured_sprite(baseAddr);
}

/* Draw mode / texture window / drawing area / mask bit handlers ---- */

static void unai_primSetting(unsigned char* baseAddr)
{
    /* Single-word command.  We forward the raw word to the same
     * dispatcher that gpulib_if.cpp uses; it picks the right E1..E6
     * branch based on the high nibble. */
    unai_gp0_setting(raw_word(baseAddr, 0));
}

#undef PRIM
#undef Blending
#undef Blending_Mode
#undef Lighting
#undef Dithering

/* ==================================================================
 * Primitive dispatch table.
 *
 * Modelled on xbox_soft's primTableJ / gpu_duck's duck_primTable.
 * Entries for opcodes that gpu_unai doesn't render itself fall back
 * to xbox_soft handlers (LoadImage / StoreImage / MoveImage) because
 * those operate on the shared psxVuw buffer.
 * ================================================================ */

extern "C" void (* const unai_primTable[256])(unsigned char* baseAddr) =
{
    /* 0x00..0x0F: NOP / clear cache / 0x02 BlockFill / others NI */
    unai_primNI, unai_primNI, unai_primBlkFill, unai_primNI,
    unai_primNI, unai_primNI, unai_primNI, unai_primNI,
    unai_primNI, unai_primNI, unai_primNI, unai_primNI,
    unai_primNI, unai_primNI, unai_primNI, unai_primNI,

    /* 0x10..0x1F: NI (irq, etc.) */
    unai_primNI, unai_primNI, unai_primNI, unai_primNI,
    unai_primNI, unai_primNI, unai_primNI, unai_primNI,
    unai_primNI, unai_primNI, unai_primNI, unai_primNI,
    unai_primNI, unai_primNI, unai_primNI, unai_primNI,

    /* 0x20..0x23: Mono 3-pt poly */
    unai_primPolyF3, unai_primPolyF3, unai_primPolyF3, unai_primPolyF3,
    /* 0x24..0x27: Tex 3-pt poly */
    unai_primPolyFT3, unai_primPolyFT3, unai_primPolyFT3, unai_primPolyFT3,
    /* 0x28..0x2B: Mono 4-pt poly */
    unai_primPolyF4, unai_primPolyF4, unai_primPolyF4, unai_primPolyF4,
    /* 0x2C..0x2F: Tex 4-pt poly */
    unai_primPolyFT4, unai_primPolyFT4, unai_primPolyFT4, unai_primPolyFT4,
    /* 0x30..0x33: Gouraud 3-pt */
    unai_primPolyG3, unai_primPolyG3, unai_primPolyG3, unai_primPolyG3,
    /* 0x34..0x37: Gouraud-Tex 3-pt */
    unai_primPolyGT3, unai_primPolyGT3, unai_primPolyGT3, unai_primPolyGT3,
    /* 0x38..0x3B: Gouraud 4-pt */
    unai_primPolyG4, unai_primPolyG4, unai_primPolyG4, unai_primPolyG4,
    /* 0x3C..0x3F: Gouraud-Tex 4-pt */
    unai_primPolyGT4, unai_primPolyGT4, unai_primPolyGT4, unai_primPolyGT4,

    /* 0x40..0x4F: Mono lines (2-pt + polyline) — polyline expansion
     * is done by xbox_soft's primLineFEx, which calls back into our
     * table at 0x40..0x47 for each segment. */
    unai_primLineF2, unai_primLineF2, unai_primLineF2, unai_primLineF2,
    unai_primNI,     unai_primNI,     unai_primNI,     unai_primNI,
    unai_primLineF2, unai_primLineF2, unai_primLineF2, unai_primLineF2,
    unai_primLineF2, unai_primLineF2, unai_primLineF2, unai_primLineF2,

    /* 0x50..0x5F: Gouraud lines */
    unai_primLineG2, unai_primLineG2, unai_primLineG2, unai_primLineG2,
    unai_primNI,     unai_primNI,     unai_primNI,     unai_primNI,
    unai_primLineG2, unai_primLineG2, unai_primLineG2, unai_primLineG2,
    unai_primLineG2, unai_primLineG2, unai_primLineG2, unai_primLineG2,

    /* 0x60..0x63: Mono rectangle (variable size) */
    unai_primTileVar, unai_primTileVar, unai_primTileVar, unai_primTileVar,
    /* 0x64..0x67: Textured rectangle (variable size) */
    unai_primSpriteVar, unai_primSpriteVar, unai_primSpriteVar, unai_primSpriteVar,
    /* 0x68..0x6B: Mono rectangle 1x1 dot */
    unai_primTile1, unai_primTile1, unai_primTile1, unai_primTile1,
    /* 0x6C..0x6F: NI */
    unai_primNI, unai_primNI, unai_primNI, unai_primNI,
    /* 0x70..0x73: Mono rectangle 8x8 */
    unai_primTile8, unai_primTile8, unai_primTile8, unai_primTile8,
    /* 0x74..0x77: Textured rectangle 8x8 */
    unai_primSprite8, unai_primSprite8, unai_primSprite8, unai_primSprite8,
    /* 0x78..0x7B: Mono rectangle 16x16 */
    unai_primTile16, unai_primTile16, unai_primTile16, unai_primTile16,
    /* 0x7C..0x7F: Textured rectangle 16x16 */
    unai_primSprite16, unai_primSprite16, unai_primSprite16, unai_primSprite16,

    /* 0x80..0x9F: VRAM-to-VRAM copy.  xbox_soft only handles 0x80
     * (the others are NI in primTableJ); we mirror that to keep
     * dispatch identical between renderers. */
    primMoveImage, unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,   unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,   unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,   unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,   unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,   unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,   unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,   unai_primNI, unai_primNI, unai_primNI,

    /* 0xA0..0xBF: CPU-to-VRAM (LoadImage).  Only 0xA0 is real. */
    primLoadImage, unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,   unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,   unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,   unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,   unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,   unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,   unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,   unai_primNI, unai_primNI, unai_primNI,

    /* 0xC0..0xDF: VRAM-to-CPU (StoreImage).  Only 0xC0 is real. */
    primStoreImage, unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,    unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,    unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,    unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,    unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,    unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,    unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,    unai_primNI, unai_primNI, unai_primNI,

    /* 0xE0: NI */
    unai_primNI,
    /* 0xE1..0xE6: draw settings (texpage, txwin, draw area, offset, mask) */
    unai_primSetting, unai_primSetting, unai_primSetting,
    unai_primSetting, unai_primSetting, unai_primSetting,
    /* 0xE7..0xEF: NI */
    unai_primNI, unai_primNI, unai_primNI, unai_primNI,
    unai_primNI, unai_primNI, unai_primNI, unai_primNI,
    unai_primNI,
    /* 0xF0..0xFF: NI */
    unai_primNI, unai_primNI, unai_primNI, unai_primNI,
    unai_primNI, unai_primNI, unai_primNI, unai_primNI,
    unai_primNI, unai_primNI, unai_primNI, unai_primNI,
    unai_primNI, unai_primNI, unai_primNI, unai_primNI
};
