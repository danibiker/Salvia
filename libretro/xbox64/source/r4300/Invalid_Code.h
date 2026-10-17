/* Invalid_Code.h - Uses 1/8th the memory as the char hash table
   by Mike Slegeir for Mupen64-GC
 */

#ifndef INVALID_CODE_H
#define INVALID_CODE_H

/* [Xbox360] `invalid_code` lo define Invalid_Code.c, que es C, y lo usan
 * Recompile.c y Wrappers.c, que son C++.  Ver la nota en gc_memory/memory.h. */
#ifdef __cplusplus
extern "C" {
#endif

extern unsigned char *const invalid_code;
#define invalid_code_get(block_num) (invalid_code[(block_num)])
#define invalid_code_set(block_num, value) (invalid_code[(block_num)] = (value))

#ifdef __cplusplus
}
#endif

#endif
