/* stdbool.h compatibility shim for MSVC 2010 (and earlier), which does not
 * ship a C99 stdbool.h. Delegate to libretro-common's boolean.h so that every
 * TU gets the same definition of bool (unsigned char for _MSC_VER < 1800).
 *
 * This directory (compat/msvc) is only added to the include path by the MSVC
 * project files, so this shim never shadows a real stdbool.h for other
 * compilers.
 */

#ifndef _STDBOOL_H
#define _STDBOOL_H

#if defined(_MSC_VER) && _MSC_VER < 1800
#include <boolean.h>
#endif

#endif
