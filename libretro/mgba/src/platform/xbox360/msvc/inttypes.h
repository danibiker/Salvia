/* inttypes.h replacement for MSVC 2010 and the Xbox 360 XDK compiler.
 * Only the format macros are provided (mGBA does not use the functions).
 * 64-bit values use the Microsoft "I64" length modifier. */
#ifndef MGBA_MSVC_INTTYPES_H
#define MGBA_MSVC_INTTYPES_H

#include <stdint.h>

#define PRId8 "d"
#define PRIi8 "i"
#define PRIu8 "u"
#define PRIx8 "x"
#define PRIX8 "X"
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

#define PRIdMAX "I64d"
#define PRIuMAX "I64u"
#define PRIxMAX "I64x"
#define PRIXMAX "I64X"

#define SCNd8 "hhd"
#define SCNu8 "hhu"
#define SCNx8 "hhx"
#define SCNd16 "hd"
#define SCNu16 "hu"
#define SCNx16 "hx"
#define SCNd32 "d"
#define SCNu32 "u"
#define SCNx32 "x"
#define SCNd64 "I64d"
#define SCNu64 "I64u"
#define SCNx64 "I64x"

#endif
