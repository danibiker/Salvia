/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * Compatibility layer for Visual Studio 2010 (cl 16.00, _MSC_VER 1600) and
 * the Xbox 360 XDK compiler (same front end).
 *
 * MSVC 2010 only supports a subset of C++11 (auto, decltype, lambdas, rvalue
 * references, nullptr, static_assert, override) and its C runtime lacks most
 * of C99 (snprintf, inttypes.h, round, ...).
 *
 * - SCUMMVM_CXX03_COMPAT selects, in the ScummVM common code, alternatives to
 *   variadic templates, alias templates and std::initializer_list.
 * - The rest of this file provides the missing C99 runtime functions.
 *
 * This header is included from portdefs.h (and so from common/scummsys.h)
 * before anything else.
 */

#ifndef LIBRETRO_MSVC2010_COMPAT_H
#define LIBRETRO_MSVC2010_COMPAT_H

#if defined(_MSC_VER) && _MSC_VER < 1700

#define SCUMMVM_CXX03_COMPAT 1
#define NO_CXX11_ALIGNAS 1

#ifdef __cplusplus
/* Warnings about the C++11 language extensions MSVC 2010 does support */
#pragma warning(disable: 4481) /* nonstandard extension used: override specifier 'override' */
#pragma warning(disable: 4482) /* nonstandard extension used: enum 'X' used in qualified name */
#pragma warning(disable: 4480) /* nonstandard extension used: specifying underlying type for enum */
#pragma warning(disable: 4351) /* new behavior: elements of array will be default initialized */
#endif
/* Noisy warnings for ScummVM code */
#pragma warning(disable: 4244 4267 4305 4800 4355 4996 4068 4146 4200 4065 4554 4018 4099)

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _CRT_NONSTDC_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#endif
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

/* SAL 2 annotations do not exist yet in MSVC 2010 */
#ifndef MSVC_PRINTF
#define MSVC_PRINTF
#endif

/* C99 __func__ */
#ifndef __func__
#define __func__ __FUNCTION__
#endif

/* <inttypes.h> is missing */
#ifndef PRId8
#define PRId8  "d"
#define PRIi8  "i"
#define PRIu8  "u"
#define PRIx8  "x"
#define PRIX8  "X"
#define PRId16 "hd"
#define PRIi16 "hi"
#define PRIu16 "hu"
#define PRIx16 "hx"
#define PRIX16 "hX"
#define PRId32 "d"
#define PRIi32 "i"
#define PRIu32 "u"
#define PRIx32 "x"
#define PRIX32 "X"
#define PRId64 "I64d"
#define PRIi64 "I64i"
#define PRIu64 "I64u"
#define PRIx64 "I64x"
#define PRIX64 "I64X"
#define PRIdPTR "Id"
#define PRIiPTR "Ii"
#define PRIuPTR "Iu"
#define PRIxPTR "Ix"
#define PRIXPTR "IX"
#define SCNd32 "d"
#define SCNu32 "u"
#define SCNx32 "x"
#define SCNd64 "I64d"
#define SCNu64 "I64u"
#define SCNx64 "I64x"
#endif

/* C99 conformant snprintf/vsnprintf (the MSVC ones do not terminate the
 * string on truncation and return -1 instead of the needed length) */
static __inline int scummvm_msvc_vsnprintf(char *buffer, size_t size, const char *format, va_list args) {
	int count = -1;
	if (size != 0)
		count = _vsnprintf_s(buffer, size, _TRUNCATE, format, args);
	if (count == -1)
		count = _vscprintf(format, args);
	return count;
}

static __inline int scummvm_msvc_snprintf(char *buffer, size_t size, const char *format, ...) {
	int count;
	va_list args;
	va_start(args, format);
	count = scummvm_msvc_vsnprintf(buffer, size, format, args);
	va_end(args);
	return count;
}

#define vsnprintf scummvm_msvc_vsnprintf
#define snprintf  scummvm_msvc_snprintf

#ifndef va_copy
#define va_copy(dst, src) ((dst) = (src))
#endif

/* 64 bit string conversion */
#ifndef strtoll
#define strtoll  _strtoi64
#endif
#ifndef strtoull
#define strtoull _strtoui64
#endif
#ifndef atoll
#define atoll    _atoi64
#endif
#undef strtof
static __inline float strtof(const char *str, char **endptr) { return (float)strtod(str, endptr); }
#ifndef llabs
#define llabs _abs64
#endif

#ifdef __cplusplus
/* MSVC 2010's <math.h> only has float/double/long double overloads (C++11 added
 * the integral ones), so e.g. pow(int, int) or sqrt(int) are ambiguous.
 * These templates only match when no exact floating point overload exists. */
#define SCUMMVM_MSVC_MATH1(f) \
	template<typename T> inline double f(T x) { return f((double)x); }
#define SCUMMVM_MSVC_MATH2(f) \
	template<typename T, typename U> inline double f(T x, U y) { return f((double)x, (double)y); }
SCUMMVM_MSVC_MATH1(sqrt)
SCUMMVM_MSVC_MATH1(log)
SCUMMVM_MSVC_MATH1(log10)
SCUMMVM_MSVC_MATH1(exp)
SCUMMVM_MSVC_MATH1(sin)
SCUMMVM_MSVC_MATH1(cos)
SCUMMVM_MSVC_MATH1(tan)
SCUMMVM_MSVC_MATH1(asin)
SCUMMVM_MSVC_MATH1(acos)
SCUMMVM_MSVC_MATH1(atan)
SCUMMVM_MSVC_MATH1(sinh)
SCUMMVM_MSVC_MATH1(cosh)
SCUMMVM_MSVC_MATH1(tanh)
SCUMMVM_MSVC_MATH1(floor)
SCUMMVM_MSVC_MATH1(ceil)
SCUMMVM_MSVC_MATH1(fabs)
SCUMMVM_MSVC_MATH2(pow)
SCUMMVM_MSVC_MATH2(atan2)
SCUMMVM_MSVC_MATH2(fmod)
#undef SCUMMVM_MSVC_MATH1
#undef SCUMMVM_MSVC_MATH2
#endif

/* C99 math (define SCUMMVM_COMPAT_NO_C99_MATH if the runtime already provides it) */
#ifndef SCUMMVM_COMPAT_NO_C99_MATH
#ifndef NAN
static __inline float scummvm_msvc_nan(void) {
	unsigned long nan = 0x7fc00000;
	return *(float *)&nan;
}
#define NAN (scummvm_msvc_nan())
#endif
#ifndef INFINITY
#define INFINITY ((float)(DBL_MAX * DBL_MAX))
#endif

/* libretro-common's compat/msvc.h may already have defined some of these as macros */
#undef round
#undef roundf
#undef lround
#undef trunc
#undef truncf
#undef log2
#undef fmin
#undef fmax
#undef fminf
#undef fmaxf
static __inline double round(double x) { return (x >= 0.0) ? floor(x + 0.5) : ceil(x - 0.5); }
static __inline float roundf(float x) { return (x >= 0.0f) ? floorf(x + 0.5f) : ceilf(x - 0.5f); }
static __inline long lround(double x) { return (long)round(x); }
static __inline long lroundf(float x) { return (long)roundf(x); }
static __inline double trunc(double x) { return (x >= 0.0) ? floor(x) : ceil(x); }
static __inline float truncf(float x) { return (x >= 0.0f) ? floorf(x) : ceilf(x); }
static __inline double rint(double x) { return floor(x + 0.5); }
static __inline float rintf(float x) { return floorf(x + 0.5f); }
static __inline long lrint(double x) { return (long)rint(x); }
static __inline long lrintf(float x) { return (long)rintf(x); }
static __inline double nearbyint(double x) { return rint(x); }
static __inline double log2(double x) { return log(x) * 1.44269504088896340736; }
static __inline float log2f(float x) { return (float)(log((double)x) * 1.44269504088896340736); }
static __inline double exp2(double x) { return pow(2.0, x); }
static __inline float exp2f(float x) { return (float)pow(2.0, (double)x); }
static __inline double cbrt(double x) { return (x < 0.0) ? -pow(-x, 1.0 / 3.0) : pow(x, 1.0 / 3.0); }
static __inline float cbrtf(float x) { return (float)cbrt((double)x); }
static __inline double fmin(double a, double b) { return (a < b) ? a : b; }
static __inline double fmax(double a, double b) { return (a > b) ? a : b; }
static __inline float fminf(float a, float b) { return (a < b) ? a : b; }
static __inline float fmaxf(float a, float b) { return (a > b) ? a : b; }
static __inline double copysign(double x, double y) { return _copysign(x, y); }
#endif /* SCUMMVM_COMPAT_NO_C99_MATH */

#endif /* _MSC_VER < 1700 */

#endif /* LIBRETRO_MSVC2010_COMPAT_H */
