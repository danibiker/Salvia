// xbox 360 endian specific

#ifndef XBOXENDIAN_H
#define XBOXENDIAN_H

#include <ppcintrinsics.h>


#ifndef FASTCALL
 #undef __fastcall
 #define __fastcall
#endif


typedef union {
  struct { UINT8 h3,h2,h,l; } b;
  struct { UINT16 h,l; } w;
  UINT32 d;
}  PAIR;

#define BURN_ENDIAN_SWAP_INT8(x)				x
#define BURN_ENDIAN_SWAP_INT16(x)				_byteswap_ushort(x)
#define BURN_ENDIAN_SWAP_INT32(x)				_byteswap_ulong(x)
#define BURN_ENDIAN_SWAP_INT64(x)				_byteswap_uint64(x)

#endif