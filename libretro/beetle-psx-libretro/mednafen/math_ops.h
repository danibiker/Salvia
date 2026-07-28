#ifndef __MDFN_MATH_OPS_H
#define __MDFN_MATH_OPS_H


#ifndef MATH_OPS_H
#define MATH_OPS_H



/* Definición ultra-segura para Visual Studio */
#if defined(_XBOX)
    #include <ppcintrinsics.h>

    static __inline uint32_t MDFN_clz(uint32_t x) {
        if (x == 0) return 32;
        // Prueba con el nombre alternativo si __cntlzw falla
        return _CountLeadingZeros(x); 
    }
#elif defined(_MSC_VER)
	#include <intrin.h>
    /* Entra aquí si es Visual Studio para Windows */
    static __inline uint32_t MDFN_clz_msvc(uint32_t x) {
        unsigned long index;
        if (_BitScanReverse(&index, x))
            return 31 - index;
        return 32;
    }
    #define MDFN_clz MDFN_clz_msvc
#else
    /* Otros compiladores */
    #define MDFN_clz(x) ((x) == 0 ? 32 : __builtin_clz(x))
#endif

#endif

//
// Result is defined for all possible inputs(including 0).
//
static INLINE unsigned MDFN_lzcount32(uint32 v)
{
#if defined(__GNUC__) || defined(__clang__) || defined(__ICC) || defined(__INTEL_COMPILER)
   return v ? __builtin_clz(v) : 32;
#elif defined(_MSC_VER)
   unsigned long idx;

   return MDFN_clz(v);
#else
   unsigned ret = 0;

   if(!v)
      return(32);

   if(!(v & 0xFFFF0000))
   {
      v <<= 16;
      ret += 16;
   }

   if(!(v & 0xFF000000))
   {
      v <<= 8;
      ret += 8;
   }

   if(!(v & 0xF0000000))
   {
      v <<= 4;
      ret += 4;
   }

   if(!(v & 0xC0000000))
   {
      v <<= 2;
      ret += 2;
   }

   if(!(v & 0x80000000))
   {
      v <<= 1;
      ret += 1;
   }

   return(ret);
#endif
}

// Source: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
// Rounds up to the nearest power of 2.
static INLINE uint32 round_up_pow2(uint32 v)
{
   v--;
   v |= v >> 1;
   v |= v >> 2;
   v |= v >> 4;
   v |= v >> 8;
   v |= v >> 16;
   v++;

   v += (v == 0);

   return(v);
}

// Some compilers' optimizers and some platforms might fubar the generated code from these macros,
// so some tests are run in...tests.cpp
#define sign_8_to_s16(_value) ((int16)(int8)(_value))
#define sign_9_to_s16(_value)  (((int16)((unsigned int)(_value) << 7)) >> 7)
#define sign_10_to_s16(_value)  (((int16)((uint32)(_value) << 6)) >> 6)
#define sign_11_to_s16(_value)  (((int16)((uint32)(_value) << 5)) >> 5)
#define sign_12_to_s16(_value)  (((int16)((uint32)(_value) << 4)) >> 4)
#define sign_13_to_s16(_value)  (((int16)((uint32)(_value) << 3)) >> 3)
#define sign_14_to_s16(_value)  (((int16)((uint32)(_value) << 2)) >> 2)
#define sign_15_to_s16(_value)  (((int16)((uint32)(_value) << 1)) >> 1)

// This obviously won't convert higher-than-32 bit numbers to signed 32-bit ;)
// Also, this shouldn't be used for 8-bit and 16-bit signed numbers, since you can
// convert those faster with typecasts...
#define sign_x_to_s32(_bits, _value) (((int32)((uint32)(_value) << (32 - (_bits)))) >> (32 - (_bits)))

#endif
