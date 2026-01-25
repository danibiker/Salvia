#pragma once
#include <stdint.h>

namespace Filter {
namespace HQ2x {

// ------------------------------------------------------------
// Constantes HQ2x
// ------------------------------------------------------------

enum {
    diff_offset = (0x440 << 21) + (0x207 << 11) + 0x407,
    diff_mask   = (0x380 << 21) + (0x1f0 << 11) + 0x3f0,
};

static uint32_t* yuvTable = 0;
static uint8_t rotate[256];

// ------------------------------------------------------------
// Tabla HQ2x (OBLIGATORIA COMPLETA)
// ------------------------------------------------------------

static const uint8_t hqTable[256] = {
  4,4,6,2,4,4,6,2,5,3,15,12,5,3,17,13,
  4,4,6,18,4,4,6,18,5,3,12,12,5,3,1,12,
  4,4,6,2,4,4,6,2,5,3,17,13,5,3,16,14,
  4,4,6,18,4,4,6,18,5,3,16,12,5,3,1,14,
  4,4,6,2,4,4,6,2,5,19,12,12,5,19,16,12,
  4,4,6,2,4,4,6,2,5,3,16,12,5,3,16,12,
  4,4,6,2,4,4,6,2,5,19,1,12,5,19,1,14,
  4,4,6,2,4,4,6,18,5,3,16,12,5,19,1,14,
  4,4,6,2,4,4,6,2,5,3,15,12,5,3,17,13,
  4,4,6,2,4,4,6,2,5,3,16,12,5,3,16,12,
  4,4,6,2,4,4,6,2,5,3,17,13,5,3,16,14,
  4,4,6,2,4,4,6,2,5,3,16,13,5,3,1,14,
  4,4,6,2,4,4,6,2,5,3,16,12,5,3,16,13,
  4,4,6,2,4,4,6,2,5,3,16,12,5,3,1,12,
  4,4,6,2,4,4,6,2,5,3,16,12,5,3,1,14,
  4,4,6,2,4,4,6,2,5,3,1,12,5,3,1,14
};

// ------------------------------------------------------------
// Inicialización
// ------------------------------------------------------------

static void initialize() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    yuvTable = new uint32_t[65536];

    for (uint32_t i = 0; i < 65536; i++) {
        uint8_t R = (i >> 11) & 0x1F;
        uint8_t G = (i >> 5)  & 0x3F;
        uint8_t B =  i        & 0x1F;

        double r = (R << 3) | (R >> 2);
        double g = (G << 2) | (G >> 4);
        double b = (B << 3) | (B >> 2);

        double y = (r + g + b) * (0.25 * (63.5 / 48.0));
        double u = ((r - b) * 0.25 + 128.0) * (7.5 / 7.0);
        double v = ((g * 2.0 - r - b) * 0.125 + 128.0) * (7.5 / 6.0);

        yuvTable[i] =
            ((uint32_t)y << 21) |
            ((uint32_t)u << 11) |
            (uint32_t)v;
    }

    for (uint32_t n = 0; n < 256; n++) {
        rotate[n] = ((n >> 2) & 0x11) | ((n << 2) & 0x88)
                  | ((n & 0x01) << 5) | ((n & 0x08) << 3)
                  | ((n & 0x10) >> 3) | ((n & 0x80) >> 5);
    }
}

// ------------------------------------------------------------
// Comparaciones HQ
// ------------------------------------------------------------

static inline bool same(uint16_t x, uint16_t y) {
    return !((yuvTable[x] - yuvTable[y] + diff_offset) & diff_mask);
}

static inline bool diff(uint32_t x, uint16_t y) {
    return ((x - yuvTable[y]) & diff_mask) != 0;
}

// ------------------------------------------------------------
// Mezcla RGB565 (CLAVE)
// ------------------------------------------------------------

static inline void grow(uint32_t &n)
{
    // duplicar componentes sin cruzarlos
    n |= n << 16;

    // máscara RGB565 expandida
    n &= 0x07E0F81F;
}

static inline uint16_t pack(uint32_t n)
{
    n &= 0x07E0F81F;
    return (uint16_t)(n | (n >> 16));
}

static inline uint16_t blend1(uint32_t A, uint32_t B) {
    grow(A); grow(B);
    return pack((A * 3 + B) >> 2);
}

static inline uint16_t blend2(uint32_t A, uint32_t B, uint32_t C) {
    grow(A); grow(B); grow(C);
    return pack((A * 2 + B + C) >> 2);
}

static inline uint16_t blend3(uint32_t A, uint32_t B, uint32_t C) {
    grow(A); grow(B); grow(C);
    return pack((A * 5 + B * 2 + C) >> 3);
}

static inline uint16_t blend4(uint32_t A, uint32_t B, uint32_t C) {
    grow(A); grow(B); grow(C);
    return pack((A * 6 + B + C) >> 3);
}

static inline uint16_t blend5(uint32_t A, uint32_t B, uint32_t C) {
    grow(A); grow(B); grow(C);
    return pack((A * 2 + (B + C) * 3) >> 3);
}

static inline uint16_t blend6(uint32_t A, uint32_t B, uint32_t C) {
    grow(A); grow(B); grow(C);
    return pack((A * 14 + B + C) >> 4);
}

// ------------------------------------------------------------
// Reglas HQ2x COMPLETAS
// ------------------------------------------------------------

static uint16_t blend(
    unsigned rule,
    uint16_t E, uint16_t A, uint16_t B,
    uint16_t D, uint16_t F, uint16_t H
) {
    switch (rule) {
        default:
        case  0: return E;
        case  1: return blend1(E, A);
        case  2: return blend1(E, D);
        case  3: return blend1(E, B);
        case  4: return blend2(E, D, B);
        case  5: return blend2(E, A, B);
        case  6: return blend2(E, A, D);
        case  7: return blend3(E, B, D);
        case  8: return blend3(E, D, B);
        case  9: return blend4(E, D, B);
        case 10: return blend5(E, D, B);
        case 11: return blend6(E, D, B);
        case 12: return same(B, D) ? blend2(E, D, B) : E;
        case 13: return same(B, D) ? blend5(E, D, B) : E;
        case 14: return same(B, D) ? blend6(E, D, B) : E;
        case 15: return same(B, D) ? blend2(E, D, B) : blend1(E, A);
        case 16: return same(B, D) ? blend4(E, D, B) : blend1(E, A);
        case 17: return same(B, D) ? blend5(E, D, B) : blend1(E, A);
        case 18: return same(B, F) ? blend3(E, B, D) : blend1(E, D);
        case 19: return same(D, H) ? blend3(E, D, B) : blend1(E, B);
    }
}

// ------------------------------------------------------------
// API pública
// ------------------------------------------------------------

void size(unsigned &width, unsigned &height) {
    width  *= 2;
    height *= 2;
}

void render(uint16_t* output, unsigned outPitch,
    const uint16_t* input, unsigned pitch,
    unsigned width, unsigned height
) {
    initialize();

    pitch    >>= 1;
    outPitch >>= 1;

    for (unsigned y = 1; y < height - 1; y++) {
        const uint16_t* in = input + y * pitch;
        uint16_t* out0 = output + (y * 2) * outPitch;
        uint16_t* out1 = out0 + outPitch;

        for (unsigned x = 1; x < width - 1; x++) {
            uint16_t A = in[-pitch - 1];
            uint16_t B = in[-pitch];
            uint16_t C = in[-pitch + 1];
            uint16_t D = in[-1];
            uint16_t E = in[0];
            uint16_t F = in[1];
            uint16_t G = in[pitch - 1];
            uint16_t H = in[pitch];
            uint16_t I = in[pitch + 1];

            uint32_t e = yuvTable[E] + diff_offset;

            uint8_t pattern = 0;
            pattern |= diff(e, A) << 0;
            pattern |= diff(e, B) << 1;
            pattern |= diff(e, C) << 2;
            pattern |= diff(e, D) << 3;
            pattern |= diff(e, F) << 4;
            pattern |= diff(e, G) << 5;
            pattern |= diff(e, H) << 6;
            pattern |= diff(e, I) << 7;

            out0[0] = blend(hqTable[pattern], E, A, B, D, F, H);
            pattern = rotate[pattern];
            out0[1] = blend(hqTable[pattern], E, C, F, B, H, D);
            pattern = rotate[pattern];
            out1[1] = blend(hqTable[pattern], E, I, H, F, D, B);
            pattern = rotate[pattern];
            out1[0] = blend(hqTable[pattern], E, G, D, H, B, F);

            in++;
            out0 += 2;
            out1 += 2;
        }
    }
}

static inline uint16_t mix565(uint16_t a, uint16_t b)
{
    uint32_t n = ((uint32_t)a + (uint32_t)b) >> 1;
    n &= 0x7BEF; // evita overflow cruzado
    return (uint16_t)n;
}


void scale2x_to_3x_565(
    const uint16_t* src, unsigned srcW, unsigned srcH, unsigned srcPitch,
    uint16_t* dst, unsigned dstPitch
) {
    srcPitch >>= 1;
    dstPitch >>= 1;

    for (unsigned y = 0; y < srcH; y += 2) {
        const uint16_t* s0 = src + y * srcPitch;
        const uint16_t* s1 = src + (y + 1) * srcPitch;

        uint16_t* d0 = dst + (y * 3 / 2) * dstPitch;
        uint16_t* d1 = d0 + dstPitch;
        uint16_t* d2 = d1 + dstPitch;

        for (unsigned x = 0; x < srcW; x += 2) {
            uint16_t A = s0[x];
            uint16_t B = s0[x + 1];
            uint16_t C = s1[x];
            uint16_t D = s1[x + 1];

            uint16_t X = mix565(mix565(A, B), mix565(C, D));

            // fila 0
            d0[0] = A; d0[1] = A; d0[2] = B;
            // fila 1
            d1[0] = A; d1[1] = X; d1[2] = B;
            // fila 2
            d2[0] = C; d2[1] = C; d2[2] = D;

            d0 += 3;
            d1 += 3;
            d2 += 3;
        }
    }
}
} // HQ2x


} // Filter

