#include <libretro/libretro.h>
#include <SDL.h>

const uint16_t SDL_TO_LIBRETRO_KEYS[SDLK_LAST] = {
	RETROK_UNKNOWN,     // 0  - SDLK_UNKNOWN
	RETROK_UNKNOWN,     // 1  - SDLK_FIRST
	RETROK_UNKNOWN,     // 2
	RETROK_UNKNOWN,     // 3
	RETROK_UNKNOWN,     // 4
	RETROK_UNKNOWN,     // 5
	RETROK_UNKNOWN,     // 6
	RETROK_UNKNOWN,     // 7
	RETROK_BACKSPACE,   // 8  - SDLK_BACKSPACE
	RETROK_TAB,         // 9  - SDLK_TAB
	RETROK_UNKNOWN,     // 10
	RETROK_UNKNOWN,     // 11
	RETROK_CLEAR,       // 12 - SDLK_CLEAR
	RETROK_RETURN,      // 13 - SDLK_RETURN  (en SDL 1.2 SDLK_RETURN == '\r' == 13)
	RETROK_UNKNOWN,     // 14
	RETROK_UNKNOWN,     // 15
	RETROK_UNKNOWN,     // 16
	RETROK_UNKNOWN,     // 17
	RETROK_UNKNOWN,     // 18
	RETROK_PAUSE,       // 19 - SDLK_PAUSE
	RETROK_UNKNOWN,     // 20
	RETROK_UNKNOWN,     // 21
	RETROK_UNKNOWN,     // 22
	RETROK_UNKNOWN,     // 23
	RETROK_UNKNOWN,     // 24
	RETROK_UNKNOWN,     // 25
	RETROK_UNKNOWN,     // 26
	RETROK_ESCAPE,      // 27 - SDLK_ESCAPE
	RETROK_UNKNOWN,     // 28
	RETROK_UNKNOWN,     // 29
	RETROK_UNKNOWN,     // 30
	RETROK_UNKNOWN,     // 31
	RETROK_SPACE,       // 32 - SDLK_SPACE
	RETROK_EXCLAIM,     // 33
	RETROK_QUOTEDBL,    // 34
	RETROK_HASH,        // 35
	RETROK_DOLLAR,      // 36
	RETROK_UNKNOWN,     // 37
	RETROK_AMPERSAND,   // 38
	RETROK_QUOTE,       // 39
	RETROK_LEFTPAREN,   // 40
	RETROK_RIGHTPAREN,  // 41
	RETROK_ASTERISK,    // 42
	RETROK_PLUS,        // 43
	RETROK_COMMA,       // 44
	RETROK_MINUS,       // 45
	RETROK_PERIOD,      // 46
	RETROK_SLASH,       // 47
	RETROK_0,           // 48
	RETROK_1,           // 49
	RETROK_2,           // 50
	RETROK_3,           // 51
	RETROK_4,           // 52
	RETROK_5,           // 53
	RETROK_6,           // 54
	RETROK_7,           // 55
	RETROK_8,           // 56
	RETROK_9,           // 57
	RETROK_COLON,       // 58
	RETROK_SEMICOLON,   // 59
	RETROK_LESS,        // 60
	RETROK_EQUALS,      // 61
	RETROK_GREATER,     // 62
	RETROK_QUESTION,    // 63
	RETROK_AT,          // 64
	RETROK_UNKNOWN,     // 65 - SDLK_WORLD_0 (no existe en LibRetro)
	RETROK_UNKNOWN,     // 66 - SDLK_WORLD_1
	RETROK_UNKNOWN,     // 67 - SDLK_WORLD_2
	RETROK_UNKNOWN,     // 68 - SDLK_WORLD_3
	RETROK_UNKNOWN,     // 69 - SDLK_WORLD_4
	RETROK_UNKNOWN,     // 70 - SDLK_WORLD_5
	RETROK_UNKNOWN,     // 71 - SDLK_WORLD_6
	RETROK_UNKNOWN,     // 72 - SDLK_WORLD_7
	RETROK_UNKNOWN,     // 73 - SDLK_WORLD_8
	RETROK_UNKNOWN,     // 74 - SDLK_WORLD_9
	RETROK_UNKNOWN,     // 75 - SDLK_WORLD_10
	RETROK_UNKNOWN,     // 76 - SDLK_WORLD_11
	RETROK_UNKNOWN,     // 77 - SDLK_WORLD_12
	RETROK_UNKNOWN,     // 78 - SDLK_WORLD_13
	RETROK_UNKNOWN,     // 79 - SDLK_WORLD_14
	RETROK_UNKNOWN,     // 80 - SDLK_WORLD_15
	RETROK_UNKNOWN,     // 81 - SDLK_WORLD_16
	RETROK_UNKNOWN,     // 82 - SDLK_WORLD_17
	RETROK_UNKNOWN,     // 83 - SDLK_WORLD_18
	RETROK_UNKNOWN,     // 84 - SDLK_WORLD_19
	RETROK_UNKNOWN,     // 85 - SDLK_WORLD_20
	RETROK_UNKNOWN,     // 86 - SDLK_WORLD_21
	RETROK_UNKNOWN,     // 87 - SDLK_WORLD_22
	RETROK_UNKNOWN,     // 88 - SDLK_WORLD_23
	RETROK_UNKNOWN,     // 89 - SDLK_WORLD_24
	RETROK_UNKNOWN,     // 90 - SDLK_WORLD_25
	RETROK_LEFTBRACKET, // 91 - SDLK_LEFTBRACKET
	RETROK_BACKSLASH,   // 92 - SDLK_BACKSLASH
	RETROK_RIGHTBRACKET,// 93 - SDLK_RIGHTBRACKET
	RETROK_CARET,       // 94 - SDLK_CARET
	RETROK_UNDERSCORE,  // 95 - SDLK_UNDERSCORE
	RETROK_BACKQUOTE,   // 96 - SDLK_BACKQUOTE
	RETROK_a,           // 97 - SDLK_a
	RETROK_b,           // 98 - SDLK_b
	RETROK_c,           // 99 - SDLK_c
	RETROK_d,           // 100 - SDLK_d
	RETROK_e,           // 101 - SDLK_e
	RETROK_f,           // 102 - SDLK_f
	RETROK_g,           // 103 - SDLK_g
	RETROK_h,           // 104 - SDLK_h
	RETROK_i,           // 105 - SDLK_i
	RETROK_j,           // 106 - SDLK_j
	RETROK_k,           // 107 - SDLK_k
	RETROK_l,           // 108 - SDLK_l
	RETROK_m,           // 109 - SDLK_m
	RETROK_n,           // 110 - SDLK_n
	RETROK_o,           // 111 - SDLK_o
	RETROK_p,           // 112 - SDLK_p
	RETROK_q,           // 113 - SDLK_q
	RETROK_r,           // 114 - SDLK_r
	RETROK_s,           // 115 - SDLK_s
	RETROK_t,           // 116 - SDLK_t
	RETROK_u,           // 117 - SDLK_u
	RETROK_v,           // 118 - SDLK_v
	RETROK_w,           // 119 - SDLK_w
	RETROK_x,           // 120 - SDLK_x
	RETROK_y,           // 121 - SDLK_y
	RETROK_z,           // 122 - SDLK_z
	RETROK_LEFTBRACE,   // 123 - SDLK_LEFTBRACE
	RETROK_BAR,         // 124 - SDLK_BAR
	RETROK_RIGHTBRACE,  // 125 - SDLK_RIGHTBRACE
	RETROK_TILDE,       // 126 - SDLK_TILDE
	RETROK_DELETE,      // 127 - SDLK_DELETE
	// Reserved / unused range (128-159) - SDL 1.2 no define keysyms aquí.
	// Estas 32 entradas son CRÍTICAS para que los índices siguientes se
	// alineen con los valores SDLK_* reales. Si se omiten, Home/End/F1/
	// flechas/modificadores devuelven RETROK_ equivocados.
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	// World keys (160-255) - SDLK_WORLD_0..SDLK_WORLD_95, no existen en LibRetro
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN,
	// Keypad (256-272)
	RETROK_KP0,         // 256 - SDLK_KP0
	RETROK_KP1,         // 257 - SDLK_KP1
	RETROK_KP2,         // 258 - SDLK_KP2
	RETROK_KP3,         // 259 - SDLK_KP3
	RETROK_KP4,         // 260 - SDLK_KP4
	RETROK_KP5,         // 261 - SDLK_KP5
	RETROK_KP6,         // 262 - SDLK_KP6
	RETROK_KP7,         // 263 - SDLK_KP7
	RETROK_KP8,         // 264 - SDLK_KP8
	RETROK_KP9,         // 265 - SDLK_KP9
	RETROK_KP_PERIOD,  // 266 - SDLK_KP_PERIOD
	RETROK_KP_DIVIDE,   // 267 - SDLK_KP_DIVIDE
	RETROK_KP_MULTIPLY, // 268 - SDLK_KP_MULTIPLY
	RETROK_KP_MINUS,    // 269 - SDLK_KP_MINUS
	RETROK_KP_PLUS,     // 270 - SDLK_KP_PLUS
	RETROK_KP_ENTER,    // 271 - SDLK_KP_ENTER
	RETROK_KP_EQUALS,   // 272 - SDLK_KP_EQUALS
	// Arrow keys (273-281)
	RETROK_UP,          // 273 - SDLK_UP
	RETROK_DOWN,        // 274 - SDLK_DOWN
	RETROK_RIGHT,       // 275 - SDLK_RIGHT
	RETROK_LEFT,        // 276 - SDLK_LEFT
	RETROK_INSERT,      // 277 - SDLK_INSERT
	RETROK_HOME,        // 278 - SDLK_HOME
	RETROK_END,         // 279 - SDLK_END
	RETROK_PAGEUP,      // 280 - SDLK_PAGEUP
	RETROK_PAGEDOWN,    // 281 - SDLK_PAGEDOWN
	// Function keys (282-296)
	RETROK_F1,          // 282 - SDLK_F1
	RETROK_F2,          // 283 - SDLK_F2
	RETROK_F3,          // 284 - SDLK_F3
	RETROK_F4,          // 285 - SDLK_F4
	RETROK_F5,          // 286 - SDLK_F5
	RETROK_F6,          // 287 - SDLK_F6
	RETROK_F7,          // 288 - SDLK_F7
	RETROK_F8,          // 289 - SDLK_F8
	RETROK_F9,          // 290 - SDLK_F9
	RETROK_F10,         // 291 - SDLK_F10
	RETROK_F11,         // 292 - SDLK_F11
	RETROK_F12,         // 293 - SDLK_F12
	RETROK_F13,         // 294 - SDLK_F13
	RETROK_F14,         // 295 - SDLK_F14
	RETROK_F15,         // 296 - SDLK_F15
	// Unknown (297-299)
	RETROK_UNKNOWN,     // 297
	RETROK_UNKNOWN,     // 298
	RETROK_UNKNOWN,     // 299
	// Lock keys (300-302)
	RETROK_NUMLOCK,     // 300 - SDLK_NUMLOCK
	RETROK_CAPSLOCK,    // 301 - SDLK_CAPSLOCK
	RETROK_SCROLLOCK,   // 302 - SDLK_SCROLLOCK
	// Modifiers (303-314)
	RETROK_RSHIFT,      // 303 - SDLK_RSHIFT
	RETROK_LSHIFT,      // 304 - SDLK_LSHIFT
	RETROK_RCTRL,       // 305 - SDLK_RCTRL
	RETROK_LCTRL,       // 306 - SDLK_LCTRL
	RETROK_RALT,        // 307 - SDLK_RALT
	RETROK_LALT,        // 308 - SDLK_LALT
	RETROK_RMETA,       // 309 - SDLK_RMETA
	RETROK_LMETA,       // 310 - SDLK_LMETA
	RETROK_LSUPER,      // 311 - SDLK_LSUPER
	RETROK_RSUPER,      // 312 - SDLK_RSUPER
	RETROK_MODE,        // 313 - SDLK_MODE
	RETROK_COMPOSE,     // 314 - SDLK_COMPOSE
	// Misc (315-323)
	RETROK_HELP,        // 315 - SDLK_HELP
	RETROK_PRINT,       // 316 - SDLK_PRINT
	RETROK_SYSREQ,      // 317 - SDLK_SYSREQ
	RETROK_BREAK,       // 318 - SDLK_BREAK
	RETROK_MENU,        // 319 - SDLK_MENU
	RETROK_POWER,       // 320 - SDLK_POWER
	RETROK_EURO,        // 321 - SDLK_EURO
	RETROK_UNDO        // 322 - SDLK_UNDO
};

inline uint16_t MapSDLKeyToLibRetro(uint16_t sdlKey) {
	if (sdlKey >= SDLK_LAST) return RETROK_UNKNOWN;
	return SDL_TO_LIBRETRO_KEYS[sdlKey];
}

// Fallback para derivar un carácter ASCII cuando SDL_EnableUNICODE(1) no
// está disponible o no funciona (algunos ports SDL — entre ellos el del
// Xbox 360 — no implementan la traducción Unicode, así que
// `event.key.keysym.unicode` siempre vale 0). Cubrimos letras y dígitos
// del layout US-QWERTY. Si el frontend funciona con SDL_EnableUNICODE
// (PC), el llamador debería preferir keysym.unicode y solo recurrir a
// este helper si unicode==0.
//
// Devuelve 0 si la tecla no es un carácter imprimible derivable (flechas,
// F1, etc.). Considera Shift y CapsLock para letras.
inline uint32_t SDLKeyToASCIIFallback(uint16_t sdlSym, uint16_t sdlMod) {
	bool shift = (sdlMod & (KMOD_LSHIFT | KMOD_RSHIFT)) != 0;
	bool caps  = (sdlMod & KMOD_CAPS) != 0;

	// Letras a-z: Shift XOR CapsLock → mayúscula.
	if (sdlSym >= SDLK_a && sdlSym <= SDLK_z) {
		uint32_t c = (uint32_t)sdlSym;
		return (shift != caps) ? (c - 32) : c;   /* 'a'-32 = 'A' */
	}
	// Dígitos 0-9. Sin Shift, devuelven el propio dígito; con Shift, los
	// símbolos del layout US-QWERTY: 1!  2@  3#  4$  5%  6^  7&  8*  9(  0)
	if (sdlSym >= SDLK_0 && sdlSym <= SDLK_9) {
		if (!shift) return (uint32_t)sdlSym;
		static const char shiftDigits[10] = { ')','!','@','#','$','%','^','&','*','(' };
		return (uint32_t)(unsigned char)shiftDigits[sdlSym - SDLK_0];
	}
	// Resto de ASCII imprimibles (espacio, signos básicos): pasan tal cual.
	// Las teclas no imprimibles (Enter, Backspace, flechas...) devuelven 0
	// porque el core las identifica vía el campo `keycode`/RETROK_*.
	if (sdlSym >= 32 && sdlSym < 127) {
		return (uint32_t)sdlSym;
	}
	return 0;
}


// Traduce los flags KMOD_* de SDL 1.2 a la mascara RETROKMOD_* que espera
// el callback de teclado de libretro. SDL diferencia LSHIFT/RSHIFT (bits
// distintos) mientras que libretro tiene un unico bit RETROKMOD_SHIFT —
// se hace OR de ambos lados. Importa para combos como Ctrl+C, Alt+F4...
// (para typing puro de letras NO importa, porque DOSBox-Pure y otros usan
// el campo `character` que SDL ya ha traducido con Shift/CapsLock).
inline uint16_t MapSDLModToLibRetro(uint16_t sdlMod) {
	uint16_t out = RETROKMOD_NONE;
	if (sdlMod & (KMOD_LSHIFT | KMOD_RSHIFT)) out |= RETROKMOD_SHIFT;
	if (sdlMod & (KMOD_LCTRL  | KMOD_RCTRL))  out |= RETROKMOD_CTRL;
	if (sdlMod & (KMOD_LALT   | KMOD_RALT))   out |= RETROKMOD_ALT;
	if (sdlMod & (KMOD_LMETA  | KMOD_RMETA))  out |= RETROKMOD_META;
	if (sdlMod & KMOD_NUM)                    out |= RETROKMOD_NUMLOCK;
	if (sdlMod & KMOD_CAPS)                   out |= RETROKMOD_CAPSLOCK;
	// SDL 1.2 no expone Scroll Lock como KMOD_*; RETROKMOD_SCROLLOCK queda a 0.
	return out;
}
