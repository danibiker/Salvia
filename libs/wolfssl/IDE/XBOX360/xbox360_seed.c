/* xbox360_seed.c - wc_GenerateSeed for Xbox 360
 * Uses XNetRandom() for cryptographic entropy when available,
 * falls back to QueryPerformanceCounter-based PRNG.
 */

#ifdef HAVE_CONFIG_H
    #include <wolfssl/wolfcrypt/settings.h>
#endif

#include <wolfssl/wolfcrypt/random.h>

#ifdef _XBOX

#ifndef WOLFSSL_IGNORE_FILE_WARN
    /* suppress misc.c-style standalone compilation warnings */
#endif

#include <xtl.h>

int wc_xbox360_GenerateSeed(OS_Seed* os, byte* output, word32 sz)
{
    (void)os;

    /* XNetRandom provides cryptographically strong random bytes
     * via Xbox 360 hardware RNG. Returns 0 on failure. */
    if (XNetRandom(output, (UINT)sz) != 0) {
        /* Fallback: use QueryPerformanceCounter as seed source
         * with a simple counter-mixing PRNG */
        LARGE_INTEGER counter;
        LARGE_INTEGER freq;
        word32 i;

        QueryPerformanceFrequency(&freq);

        for (i = 0; i < sz; i++) {
            QueryPerformanceCounter(&counter);
            /* Mix counter with index to break patterns */
            output[i] = (byte)((counter.LowPart ^ (i * 0x9E3779B9)) & 0xFF);
            /* Small busy-wait to change counter value */
            {
                volatile word32 j;
                for (j = 0; j < 16; j++) { }
            }
        }
    }

    return 0;
}

#endif /* _XBOX */
