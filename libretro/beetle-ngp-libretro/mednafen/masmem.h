#ifndef __MDFN_PSX_MASMEM_H
#define __MDFN_PSX_MASMEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static INLINE uint16_t LoadU16_RBO(const uint16_t *a)
{
#ifdef ARCH_POWERPC
   uint16_t tmp;
   __asm__ ("lhbrx %0, %y1" : "=r"(tmp) : "Z"(*a));
   return(tmp);
#else
   return((*a << 8) | (*a >> 8));
#endif
}

static INLINE void StoreU16_RBO(uint16_t *a, const uint16_t v)
{
#ifdef ARCH_POWERPC
   __asm__ ("sthbrx %0, %y1" : : "r"(v), "Z"(*a));
#else
   uint16_t tmp = (v << 8) | (v >> 8);
   *a = tmp;
#endif
}

#ifdef __cplusplus
}
#endif

#endif
