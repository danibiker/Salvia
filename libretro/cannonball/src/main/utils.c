/***************************************************************************
    General C Helper Functions

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdio.h>
#include <compat/msvc.h>
#include "utils.h"

/* Returns a pointer to a rotating static buffer; safe for the one
   conversion-per-expression usage in the HUD and menu code. */
const char* Utils_to_string(int i)
{
    static char buf[4][16];
    static int  n = 0;
    char* b = buf[n++ & 3];
    snprintf(b, sizeof(buf[0]), "%d", i);
    return b;
}

const char* Utils_to_hex_string(int i)
{
    static char buf[4][16];
    static int  n = 0;
    char* b = buf[n++ & 3];
    snprintf(b, sizeof(buf[0]), "%x", i);
    return b;
}
