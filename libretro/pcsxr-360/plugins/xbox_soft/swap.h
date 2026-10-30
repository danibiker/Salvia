#include <stdint.h>
#include "psxcommon.h"

// byteswappings
#ifdef _XBOX
#define SWAP16(x) _byteswap_ushort(x)
#define SWAP32(x) _byteswap_ulong(x)
#elif defined(_MSC_VER)
/* [Windows, 2026-10-30] MSVC en x86: aqui SWAP16/32 solo lo usarian HOST2BE y
 * BE2HOST (sin uso).  Si psxmem.h ya puso su identidad (little-endian), esa
 * se queda: es la que necesitan psxMu16 y compania. */
#ifndef SWAP16
#define SWAP16(x) _byteswap_ushort(x)
#endif
#ifndef SWAP32
#define SWAP32(x) _byteswap_ulong(x)
#endif
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

/* [Windows, 2026-10-30] GETLE16D en little-endian: lectura directa.
 * Lee dos pixeles de la VRAM en una palabra que BlitScreen16 convierte y
 * vuelve a escribir entera.  En la Xbox el resultado se escribe big-endian,
 * y por eso alli se rota (el pixel de la izquierda va en la mitad alta).  En
 * x86 la escritura es little-endian y el pixel de la izquierda tiene que
 * quedar en la mitad baja, que es justo como esta en memoria.  Rotar aqui
 * intercambiaba cada pareja de pixeles (tiras verticales en RGB565). */
#ifndef GETLE16D
#define GETLE16D(X) (*(const uint32_t *)(X))
#endif

#define PUTLE16(X, Y) do{*((uint16_t *)X)=HOST2LE16((uint16_t)Y);}while(0)
#define PUTLE32(X, Y) do{*((uint32_t *)X)=HOST2LE16((uint32_t)Y);}while(0)
#endif



