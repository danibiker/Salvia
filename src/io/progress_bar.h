#pragma once
// ============================================================
//  progress_bar.h — SDL 1.2 · Game-style loading bar
//
//  API pública (una sola llamada por frame):
//
//    void ProgressBar_draw(SDL_Surface* screen, float fillPercent);
//
//  fillPercent : valor entre 0.0f y 1.0f
//  El módulo gestiona su propio estado interno (shimmer, pulso,
//  tiempo) y sólo necesita ser llamado una vez por frame.
// ============================================================

#include <SDL.h>

// ════════════════════════════════════════════════════════════
//  § 3  UTILIDADES INTERNAS
// ════════════════════════════════════════════════════════════
namespace PBUtil {

    static inline Uint32 rgb(SDL_Surface* s, Uint8 r, Uint8 g, Uint8 b) {
        return SDL_MapRGBA(s->format, r, g, b, 0xFF);
    }

    static inline void fillRect(SDL_Surface* s, int x, int y, int w, int h, Uint32 color) {
        if (w <= 0 || h <= 0) return;
        SDL_Rect r = { (Sint16)x, (Sint16)y, (Uint16)w, (Uint16)h };
        SDL_FillRect(s, &r, color);
    }

    static inline SDL_Color lerpColor(SDL_Color a, SDL_Color b, float t) {
        SDL_Color c;
        c.r = (Uint8)(a.r + (b.r - a.r) * t);
        c.g = (Uint8)(a.g + (b.g - a.g) * t);
        c.b = (Uint8)(a.b + (b.b - a.b) * t);
        c.unused = 0xFF;
        return c;
    }

} // namespace PBUtil

// ════════════════════════════════════════════════════════════
//  § 6  TEXTO PIXELADO (dígitos 0-9 y %)
// ════════════════════════════════════════════════════════════
static const Uint8 PB_FONT[11][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, // 0
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, // 1
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}, // 2
    {0x1F,0x01,0x02,0x06,0x01,0x11,0x0E}, // 3
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, // 4
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, // 5
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, // 6
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, // 7
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, // 8
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, // 9
    {0x18,0x19,0x02,0x04,0x08,0x03,0x03}, // %
};

static void PB_drawDigit(SDL_Surface* s, int digit, int x, int y, int scale, Uint32 col) {
    using namespace PBUtil;
    if (digit < 0 || digit > 10) return;
    for (int row = 0; row < 7; row++)
        for (int c = 0; c < 5; c++)
            if (PB_FONT[digit][row] & (0x10 >> c))
                fillRect(s, x + c*scale, y + row*scale, scale, scale, col);
}

static void PB_drawPercent(SDL_Surface* s, int value, int cx, int cy, int scale, Uint32 col) {
    int digits  = (value >= 100) ? 3 : (value >= 10 ? 2 : 1);
    int totalW  = (digits + 1) * 6 * scale;
    int startX  = cx - totalW / 2;
    int y       = cy - 7 * scale / 2;

    if (value >= 100) {
        PB_drawDigit(s, 1,        startX,          y, scale, col);
        PB_drawDigit(s, 0,        startX + 6*scale, y, scale, col);
        PB_drawDigit(s, 0,        startX +12*scale, y, scale, col);
        PB_drawDigit(s, 10/*%*/,  startX +18*scale, y, scale, col);
    } else if (value >= 10) {
        PB_drawDigit(s, value/10, startX,          y, scale, col);
        PB_drawDigit(s, value%10, startX + 6*scale, y, scale, col);
        PB_drawDigit(s, 10,       startX +12*scale, y, scale, col);
    } else {
        PB_drawDigit(s, value,    startX,          y, scale, col);
        PB_drawDigit(s, 10,       startX + 6*scale, y, scale, col);
    }
}
