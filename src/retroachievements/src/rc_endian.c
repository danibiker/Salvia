/*
 * rc_endian.c - Big-endian host helpers for rcheevos
 *
 * Provides rc_peek_value_be(), a variant of rc_peek_value() for clients
 * whose peek callback performs *native multi-byte reads* on a big-endian
 * host.  If your peek callback reads memory byte-by-byte (the default
 * recommended approach), you do NOT need this file — the regular
 * rc_peek_value() is already endian-neutral.
 *
 * See rc_endian.h for full documentation.
 */

#define RC_ENDIAN_NEED_PEEK_DECL 1
#include "rc_endian.h"
#include "rcheevos/rc_internal.h"

#if RC_HOST_BIG_ENDIAN

/*
 * rc_peek_value_be()
 *
 * Same contract as rc_peek_value() but corrects for a peek callback that
 * returns multi-byte values in the host's native (big-endian) byte order
 * instead of little-endian order.
 *
 * The correction is applied only for the multi-byte LE sizes:
 *   RC_MEMSIZE_16_BITS, RC_MEMSIZE_24_BITS, RC_MEMSIZE_32_BITS
 * All other sizes are single-byte or already handled with explicit byte
 * extraction, so they require no swap.
 *
 * The explicit _BE sizes (RC_MEMSIZE_16_BITS_BE etc.) are NOT swapped here
 * because rc_transform_memref_value() will apply its own swap later.
 */
uint32_t rc_peek_value_be(uint32_t address, uint8_t size, rc_peek_t peek, void* ud)
{
    uint32_t value;

    if (!peek)
        return 0;

    switch (size)
    {
        /* 8-bit: endianness irrelevant */
        case RC_MEMSIZE_8_BITS:
            return peek(address, 1, ud);

        /* 16-bit LE read: native BE read returns bytes swapped */
        case RC_MEMSIZE_16_BITS:
            value = peek(address, 2, ud);
            return RC_LE16(value);

        /* 32-bit LE read: native BE read returns bytes swapped */
        case RC_MEMSIZE_32_BITS:
            value = peek(address, 4, ud);
            return RC_LE32(value);

        /* 24-bit LE: rcheevos asks for 4 bytes (rc_memref_shared_sizes maps
         * 24-bit → 32-bit), so swap the 32-bit read and then mask off the
         * top byte.  The mask is applied by rc_peek_value's default branch
         * so we reproduce that logic here. */
        case RC_MEMSIZE_24_BITS:
            value = peek(address, 4, ud);
            return RC_LE32(value) & 0x00FFFFFFu;

        default:
        {
            /* For all other sizes (bit flags, nibbles, _BE variants, floats …)
             * fall through to the standard implementation which itself calls
             * back into us for the base size. */
            const size_t index = (size_t)size;
            const uint8_t* shared = NULL; /* use rc_memref_shared_size */

            /* Re-use the same size table as the original function. */
            value = rc_peek_value_be(address,
                                     rc_memref_shared_size(size),
                                     peek, ud);
            return value & rc_memref_mask(size);
        }
    }
}

#endif /* RC_HOST_BIG_ENDIAN */
