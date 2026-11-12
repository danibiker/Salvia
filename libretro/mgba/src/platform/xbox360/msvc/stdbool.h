/* stdbool.h replacement for MSVC 2010 and the Xbox 360 XDK compiler, which
 * only implement C89 and ship no <stdbool.h>.
 *
 * `bool` is defined exactly as libretro.h does for these compilers
 * (see src/platform/libretro/libretro.h), so both headers can be included
 * in any order and the libretro API keeps the 1-byte `bool` the C++ frontend
 * expects.
 *
 * NOTE: unlike C99 `_Bool`, `unsigned char` does not normalise values to
 * 0/1. Every implicit integer/pointer -> bool conversion in the mGBA sources
 * used by this project has been rewritten as `!!(expr)` for that reason;
 * keep doing so in new code. */
#ifndef MGBA_MSVC_STDBOOL_H
#define MGBA_MSVC_STDBOOL_H

#ifndef __cplusplus
#define bool unsigned char
#define true 1
#define false 0
#define __bool_true_false_are_defined 1
#endif

#endif
