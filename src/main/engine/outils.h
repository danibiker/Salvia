/***************************************************************************
    OutRun Utility Functions & Assembler Helper Functions. 

    Common OutRun library functions.
    Helper functions used to facilitate 68K to C++ porting process.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>

extern const uint8_t DEC_TO_HEX[];
void outils_reset_random_seed();
uint32_t outils_random();
int32_t outils_isqrt(int32_t);
uint16_t outils_convert16_dechex(uint16_t);
uint32_t outils_bcd_add(uint32_t, uint32_t);
uint32_t outils_bcd_sub(uint32_t, uint32_t);
void outils_convert_counter_to_time(uint16_t counter, uint8_t* converted);
int32_t outils_next(int32_t, int32_t);
int32_t outils_abs(int32_t);
static void outils_move16(uint32_t src, uint32_t* dst){
        *dst = (*dst & 0xFFFF0000) + (src & 0xFFFF);
    }
static void outils_add16(uint32_t src, uint32_t* dst){
        *dst = (*dst & 0xFFFF0000) + (((*dst & 0xFFFF) + (src & 0xFFFF)) & 0xFFFF);
    }
static void outils_sub16(int32_t src, int32_t* dst){
        *dst = (*dst & 0xFFFF0000) + (((*dst & 0xFFFF) - (src & 0xFFFF)) & 0xFFFF);
    }
static void outils_swap32(int32_t* v){
        *v = ((*v & 0xFFFF0000) >> 16) + ((*v & 0xFFFF) << 16);
    }
static void outils_swap32u(uint32_t* v){
        *v = ((*v & 0xFFFF0000) >> 16) + ((*v & 0xFFFF) << 16);
    }

#ifdef __cplusplus
}
#endif
