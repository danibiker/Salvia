#ifndef RC_ENDIAN_H
#define RC_ENDIAN_H

/*
 * rc_endian.h - Big-endian host support for rcheevos
 *
 * This header detects host endianness at compile time and provides
 * byte-swap utilities. All multi-byte memory reads in rcheevos assume
 * the emulated system stores data in little-endian order (as most retro
 * consoles do). When rcheevos itself runs on a big-endian host (e.g.
 * Xbox 360 / PowerPC Xenon), raw multi-byte reads from the emulated
 * memory buffer will return bytes in the wrong order unless we swap them.
 *
 * Usage
 * -----
 *  - Define RC_HOST_BIG_ENDIAN=1 in your build system, OR let the
 *    auto-detection below handle it from standard compiler macros.
 *  - Include this header wherever rc_peek_value() is called (memref.c).
 *  - Use RC_LE16, RC_LE32 to convert a little-endian value read from
 *    an emulated buffer into the host's native representation.
 *    On a little-endian host these macros are no-ops.
 *
 * Big-endian "BE" sizes (RC_MEMSIZE_16_BITS_BE, etc.)
 * ----------------------------------------------------
 * rcheevos already has explicit big-endian memory size types. Those are
 * for emulated systems whose memory is big-endian (e.g. N64, Saturn).
 * The swaps below are a *separate* concern: they fix the case where the
 * *host CPU* is big-endian and reads what is supposed to be a
 * little-endian value from the emulated memory buffer.
 */

#include "rc_export.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* 1. Host endianness detection                                        */
/* ------------------------------------------------------------------ */

#if !defined(RC_HOST_BIG_ENDIAN)

  /* GCC / Clang built-in */
  #if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
    #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
      #define RC_HOST_BIG_ENDIAN 1
    #endif

  /* GCC legacy macros */
  #elif defined(__BIG_ENDIAN__) || defined(__ARMEB__) || defined(__MIPSEB__) || \
        defined(__ppc__) || defined(__ppc64__) || defined(__powerpc__) || \
        defined(__powerpc64__) || defined(_ARCH_PPC) || defined(_ARCH_PPC64)
    #define RC_HOST_BIG_ENDIAN 1

  /* Xbox 360 (PowerPC Xenon) specific */
  #elif defined(_XBOX) || defined(_XENON)
    #define RC_HOST_BIG_ENDIAN 1

  /* <endian.h> on Linux/glibc */
  #elif defined(__linux__) || defined(__GLIBC__)
    #include <endian.h>
    #if defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && (__BYTE_ORDER == __BIG_ENDIAN)
      #define RC_HOST_BIG_ENDIAN 1
    #endif

  /* BSD */
  #elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    #include <sys/endian.h>
    #if defined(_BYTE_ORDER) && defined(_BIG_ENDIAN) && (_BYTE_ORDER == _BIG_ENDIAN)
      #define RC_HOST_BIG_ENDIAN 1
    #endif
  #endif

#endif /* !RC_HOST_BIG_ENDIAN */

/* Ensure RC_HOST_BIG_ENDIAN is always defined (0 or 1) */
#ifndef RC_HOST_BIG_ENDIAN
  #define RC_HOST_BIG_ENDIAN 0
#endif

#include <rc_compat.h>
/* ------------------------------------------------------------------ */
/* 2. Byte-swap primitives                                            */
/* ------------------------------------------------------------------ */

/* Use compiler built-ins when available to generate a single BSWAP
 * instruction on x86, or equivalent on other architectures. */

#if RC_HOST_BIG_ENDIAN

  static RC_INLINE uint16_t rc_bswap16(uint16_t v) {
  #if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(v);
  #elif defined(_MSC_VER)
    return _byteswap_ushort(v);
  #else
    return (uint16_t)(((v & 0x00FFu) << 8) |
                      ((v & 0xFF00u) >> 8));
  #endif
  }

  static RC_INLINE uint32_t rc_bswap32(uint32_t v) {
  #if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(v);
  #elif defined(_MSC_VER)
    return _byteswap_ulong(v);
  #else
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) <<  8) |
           ((v & 0x00FF0000u) >>  8) |
           ((v & 0xFF000000u) >> 24);
  #endif
  }

  /*
   * RC_LE16 / RC_LE32
   * Convert a value that was assembled as little-endian bytes into the
   * host's native (big-endian) representation.
   *
   * Example: peek() returned 0x78563412 (bytes read as 0x12,0x34,0x56,0x78
   * in memory order on a LE host → native LE value 0x12345678).
   * On a BE host peek() returns the bytes in reverse: native BE reads them
   * as 0x12345678 already — but the existing rc_peek_value() code has
   * *built* the uint32_t by shifting bytes [b0 | b1<<8 | b2<<16 | b3<<24]
   * which on a BE host still produces the correct little-endian integer, so
   * no extra swap is needed at that layer.
   *
   * The real issue is when the CLIENT'S peek callback returns a multi-byte
   * value via a native multi-byte read (e.g. *(uint16_t*)ptr). In that case
   * the BE host will byte-swap the value relative to what a LE host would see.
   * RC_LE16/RC_LE32 fix that up.
   *
   * If your peek callback reads byte-by-byte (which rcheevos recommends),
   * rc_peek_value already assembles the bytes in the correct LE order and
   * no swapping is required. These macros are provided for peek callbacks
   * that perform bulk/native reads.
   */
  #define RC_LE16(v) rc_bswap16((uint16_t)(v))
  #define RC_LE32(v) rc_bswap32((uint32_t)(v))

#else /* little-endian host – no-ops */

  #define RC_LE16(v) ((uint16_t)(v))
  #define RC_LE32(v) ((uint32_t)(v))

#endif /* RC_HOST_BIG_ENDIAN */

/* ------------------------------------------------------------------ */
/* 3. rc_peek_value_be – a drop-in replacement for rc_peek_value()    */
/*    when the CLIENT peek callback does NATIVE multi-byte reads.     */
/* ------------------------------------------------------------------ */

/*
 * If your peek callback reads bytes one at a time (the safest and most
 * portable approach), rc_peek_value() in memref.c already works correctly
 * on both LE and BE hosts — the byte assembly loop is endian-neutral.
 *
 * If your callback does native multi-byte reads (e.g. reads a uint32_t
 * directly from emulated RAM), then on a BE host it will return bytes in
 * BE order. In that case call rc_peek_value_be() instead of rc_peek_value()
 * from your rc_runtime / rc_client integration code.
 */

#if defined(RC_ENDIAN_NEED_PEEK_DECL) && RC_HOST_BIG_ENDIAN
  /* Forward declaration; implementation lives in rc_endian.c */
  #include "rc_runtime_types.h"
  RC_BEGIN_C_DECLS
  uint32_t rc_peek_value_be(uint32_t address, uint8_t size, rc_peek_t peek, void* ud);
  RC_END_C_DECLS
#endif

/* ------------------------------------------------------------------ */
/* 4. Convenience: rc_runtime_progress serialisation byte order       */
/* ------------------------------------------------------------------ */

/*
 * runtime_progress.c serialises state to a byte buffer using explicit
 * byte-by-byte writes/reads (rc_runtime_progress_write_uint /
 * rc_runtime_progress_read_uint). Those functions already write in
 * little-endian format regardless of host, so no changes are needed
 * for serialisation correctness. The macros below are kept here as
 * documentation and future-proofing.
 */

/* RC_PROGRESS_IS_PORTABLE: serialised progress buffers are always LE */
#define RC_PROGRESS_IS_PORTABLE 1

#endif /* RC_ENDIAN_H */
