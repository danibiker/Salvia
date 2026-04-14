#include <stdint.h>
#include "psxcommon.h"

// byteswappings
#ifdef _XBOX
#define SWAP16(x) _byteswap_ushort(x)
#define SWAP32(x) _byteswap_ulong(x)
#else
#define SWAP16(x) ({ uint16_t y=(x); (((y)>>8 & 0xff) | ((y)<<8 & 0xff00)); })
#define SWAP32(x) ({ uint32_t y=(x); (((y)>>24 & 0xfful) | ((y)>>8 & 0xff00ul) | ((y)<<8 & 0xff0000ul) | ((y)<<24 & 0xff000000ul)); })
#endif

#ifdef __BIG_ENDIAN__

// big endian config
#define HOST2LE32(x) SWAP32(x)
#define HOST2BE32(x) (x)
#define LE2HOST32(x) SWAP32(x)
#define BE2HOST32(x) (x)

#define HOST2LE16(x) SWAP16(x)
#define HOST2BE16(x) (x)
#define LE2HOST16(x) SWAP16(x)
#define BE2HOST16(x) (x)

u32 x, amount;

#define rotl32(x, amount) (x << amount) | (x >> ((32 - amount) & 31))

static __inline u32 ppcmask(u32 mb, u32 me)
{
  u32 maskmb = ~0u >> mb;
  u32 maskme = ~0u << (31 - me);
  return (mb <= me) ? maskmb & maskme : maskmb | maskme;
}

static __inline u32 __rlwinm(u32 rs, u32 sh, u32 mb, u32 me)
{
  return rotl32(rs, sh) & ppcmask(mb, me);
}

#define GETLE16D(X) ((unsigned long)__rlwinm(GETLE32(X), 16, 0, 31))

#else

// little endian config
#define HOST2LE32(x) (x)
#define HOST2BE32(x) SWAP32(x)
#define LE2HOST32(x) (x)
#define BE2HOST32(x) SWAP32(x)

#define HOST2LE16(x) (x)
#define HOST2BE16(x) SWAP16(x)
#define LE2HOST16(x) (x)
#define BE2HOST16(x) SWAP16(x)

#endif

#define GETLEs16(X) ((int16_t)GETLE16((uint16_t *)X))
#define GETLEs32(X) ((int16_t)GETLE32((uint16_t *)X))

#if defined(__PPC__) && defined(__BIG_ENDIAN__)

#define GETLE16(X) __loadshortbytereverse(0,(X))
#define GETLE32(X) __loadwordbytereverse(0,(X))
#define  PUTLE16(X,Y) __storeshortbytereverse(Y,0,(X));
#define  PUTLE32(X,Y) __storewordbytereverse(Y,0,(X));

#else
#define GETLE16(X) LE2HOST16(*(uint16_t *)X)
#define GETLE32(X) LE2HOST32(*(uint32_t *)X)

#define PUTLE16(X, Y) do{*((uint16_t *)X)=HOST2LE16((uint16_t)Y);}while(0)
#define PUTLE32(X, Y) do{*((uint32_t *)X)=HOST2LE16((uint32_t)Y);}while(0)
#endif



