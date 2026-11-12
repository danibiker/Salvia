/* C99 pieces that MSVC 2010 and the Xbox 360 XDK compiler (both C89-only)
 * do not provide. Included from include/mgba-util/common.h when
 * MGBA_MSVC_C89 is defined; this folder must also be on the include path so
 * that <stdbool.h> and <inttypes.h> resolve to the replacements next to this
 * file.
 *
 * Everything here is portable between Win32 (x86) and Xbox 360 (PowerPC). */
#ifndef MGBA_MSVC_COMPAT_H
#define MGBA_MSVC_COMPAT_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* C99 keywords */
#ifndef inline
#define inline __inline
#endif

/* C11 _Static_assert (used at file scope and right after declarations).
 * Implemented as a typedef so it stays a declaration in C89. */
#define MGBA_C89_CONCAT2(A, B) A ## B
#define MGBA_C89_CONCAT(A, B) MGBA_C89_CONCAT2(A, B)
#define static_assert(X, C) typedef char MGBA_C89_CONCAT(mgba_static_assert_, __COUNTER__)[(X) ? 1 : -1]

/* snprintf/vsnprintf: the CRT only has _snprintf/_vsnprintf, which do not
 * NUL-terminate on truncation and return -1. These wrappers behave like C99
 * (always terminated, return the length that would have been written). */
int mgba_c89_vsnprintf(char* str, size_t size, const char* format, va_list args);
int mgba_c89_snprintf(char* str, size_t size, const char* format, ...);
#define snprintf mgba_c89_snprintf
#define vsnprintf mgba_c89_vsnprintf

/* C99 <stdlib.h> */
#define strtof(STR, END) ((float) strtod((STR), (END)))
#ifndef strtoll
#define strtoll _strtoi64
#endif
#ifndef strtoull
#define strtoull _strtoui64
#endif

/* C99 <math.h> float functions used by mGBA */
static __inline float mgba_c89_fminf(float a, float b) {
	return a < b ? a : b;
}
static __inline float mgba_c89_fmaxf(float a, float b) {
	return a > b ? a : b;
}
static __inline float mgba_c89_exp2f(float x) {
	return (float) pow(2.0, (double) x);
}
static __inline float mgba_c89_cbrtf(float x) {
	return x < 0 ? (float) -pow((double) -x, 1.0 / 3.0) : (float) pow((double) x, 1.0 / 3.0);
}
static __inline float mgba_c89_hypotf(float x, float y) {
	return (float) sqrt((double) x * x + (double) y * y);
}
#define fminf mgba_c89_fminf
#define fmaxf mgba_c89_fmaxf
#define exp2f mgba_c89_exp2f
#define cbrtf mgba_c89_cbrtf
#define hypotf mgba_c89_hypotf

#endif
