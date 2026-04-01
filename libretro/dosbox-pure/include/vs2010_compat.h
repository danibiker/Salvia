/*
 * VS2010 Compatibility Header
 *
 * Visual Studio 2010 (MSVC 16.0, _MSC_VER 1600) does not fully support C++11.
 * This header provides workarounds for C++11 features used in the codebase:
 *   - override / final keywords
 *   - nullptr
 *   - static_assert
 *   - Range-based for loops have been converted to traditional for loops
 *   - In-class member initializers must be moved to constructors
 *
 * Xbox 360 XDK also uses the VS2010 compiler, so this header applies there too.
 */

#ifndef DBP_VS2010_COMPAT_H
#define DBP_VS2010_COMPAT_H

#if defined(_MSC_VER) && _MSC_VER < 1700

/* override and final are not supported in VS2010 */
#define override
#define final

/* nullptr is not a keyword in VS2010; it was added in VS2010 SP1 as a partial
   extension, but to be safe we define it if not already available */
#ifndef nullptr
#define nullptr 0
#endif

/* static_assert is available in VS2010 as a keyword, no workaround needed */

/* VS2010 does not provide <inttypes.h> with PRI format macros */
#ifndef PRId32
#define PRId32 "d"
#endif
#ifndef PRId64
#define PRId64 "I64d"
#endif
#ifndef PRIu32
#define PRIu32 "u"
#endif
#ifndef PRIu64
#define PRIu64 "I64u"
#endif
#ifndef PRIx64
#define PRIx64 "I64x"
#endif
#ifndef PRIX64
#define PRIX64 "I64X"
#endif

#endif /* _MSC_VER < 1700 */

/*
 * Xbox 360 (XDK) specific: force big-endian and PowerPC defines
 */
#if defined(_XBOX)
# if !defined(WORDS_BIGENDIAN)
#  define WORDS_BIGENDIAN 1
# endif
# if !defined(__powerpc__) && !defined(__POWERPC__) && !defined(_ARCH_PPC)
#  define __POWERPC__ 1
#  define _ARCH_PPC 1
# endif
/* Xbox 360 does not support mprotect */
# undef C_HAVE_MPROTECT
/* Xbox 360 does not have standard Win32 filesystem APIs in the same way */
# if !defined(_XBOX_FILESYSTEM)
#  define _XBOX_FILESYSTEM 1
# endif
#endif

#ifdef _XBOX
	#include <xtl.h>
	#include <ppcintrinsics.h>

	static inline unsigned char _BitScanReverse(unsigned long* Index, unsigned long Mask) {
        if (Mask == 0) return 0;
        // _CountLeadingZeros es el intrínseco oficial del XDK
        *Index = 31 - _CountLeadingZeros(Mask);
        return 1;
    }

    static inline unsigned char _BitScanForward(unsigned long* Index, unsigned long Mask) {
		unsigned long val;
        if (Mask == 0) return 0;
        // Lógica para encontrar el bit menos significativo
        val = Mask & (unsigned long)(-(long)Mask);
        *Index = 31 - _CountLeadingZeros(val);
        return 1;
    }
#endif

#endif /* DBP_VS2010_COMPAT_H */
