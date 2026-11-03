// One cached-source read produces the tiled atlas and exact bit-29 metadata.
#ifndef FBNEO_EPIC12_GPU_TILE_ALPHA_H
#define FBNEO_EPIC12_GPU_TILE_ALPHA_H
#include "epic12_gpu_tile.h"
#include "epic12_gpu_alpha.h"

#if defined(EPIC12_ALPHA_VECTOR) && (defined(_XBOX) || defined(EPIC12_GPU_TILE_VECTOR_TEST))
#if defined(_XBOX)
#define EPIC12_TILE_ALPHA_INLINE __forceinline
static bool epic12_tile_alpha_enabled=false;
#else
#define EPIC12_TILE_ALPHA_INLINE inline __attribute__((always_inline))
static bool epic12_tile_alpha_enabled=true;
#endif

// A contiguous 128-byte tiled strip holds sixteen pixels from each of two
// rows, interleaved in groups of four. Keep its loads in registers for alpha.
static EPIC12_TILE_ALPHA_INLINE void epic12_gpu_tile_alpha_strip(unsigned char* destination,
 const uint32_t* row0,const uint32_t* row1,const Epic12GpuAlphaVector& vector,
 __vector4& alpha0,__vector4& alpha1)
{
 __vector4 v0=__lvx(row0,0),v1=__lvx(row1,0);
 __vector4 v2=__lvx(row0,16),v3=__lvx(row1,16);
 __vector4 v4=__lvx(row0,32),v5=__lvx(row1,32);
 __vector4 v6=__lvx(row0,48),v7=__lvx(row1,48);
 __stvx_volatile(v0,destination,0);__stvx_volatile(v1,destination,16);
 __stvx_volatile(v2,destination,32);__stvx_volatile(v3,destination,48);
 __stvx_volatile(v4,destination,64);__stvx_volatile(v5,destination,80);
 __stvx_volatile(v6,destination,96);__stvx_volatile(v7,destination,112);
 alpha0=vector.half(v0,v2,v4,v6);alpha1=vector.half(v1,v3,v5,v7);
}

template<int First>
static EPIC12_TILE_ALPHA_INLINE void epic12_gpu_tile_alpha_group(
 unsigned char* &destination,const uint32_t* source,int pitch,
 Epic12GpuAlphaPage& alpha,int pageY,int word,const Epic12GpuAlphaVector& vector,
 int group)
{
 const int base_y = group * 8;
 __vector4 summary;
 for(int p = 0; p < 4; ++p) {
  int y = base_y + p * 2;
#if defined(_XBOX)
  if(y + 2 < 32) { __dcbt(0, source + (y + 2) * pitch); __dcbt(0, source + (y + 3) * pitch); }
#endif
  const uint32_t* row0 = source + y * pitch;
  const uint32_t* row1 = row0 + pitch;
  __vector4 a0, a1, b0, b1;
  epic12_gpu_tile_alpha_strip(destination, row0 + First, row1 + First, vector, a0, a1);
  epic12_gpu_tile_alpha_strip(destination + 128, row0 + (First ^ 16), row1 + (First ^ 16), vector, b0, b1);
  __vector4 mask0, mask1;
  if(First) { mask0 = __vperm(b0, a0, vector.pack); mask1 = __vperm(b1, a1, vector.pack); }
  else { mask0 = __vperm(a0, b0, vector.pack); mask1 = __vperm(a1, b1, vector.pack); }
  __stvewx(mask0, &alpha.rows[pageY + y][word], 0);
  __stvewx(mask1, &alpha.rows[pageY + y + 1][word], 0);
  __vector4 pair = __vor(mask0, mask1);
  if(p == 0) summary = pair;
  else summary = __vor(summary, pair);
  destination += 256;
 }
 __stvewx(summary, &alpha.groups[(pageY + base_y) / 8][word], 0);
}

static void epic12_gpu_tile_alpha_block(unsigned char* destination,const uint32_t* source,
 int pitch,Epic12GpuAlphaPage& alpha,int pageY,int word,const Epic12GpuAlphaVector& vector)
{
 epic12_gpu_tile_alpha_group<0>(destination, source, pitch, alpha, pageY, word, vector, 0);
 epic12_gpu_tile_alpha_group<16>(destination, source, pitch, alpha, pageY, word, vector, 1);
 epic12_gpu_tile_alpha_group<0>(destination, source, pitch, alpha, pageY, word, vector, 2);
 epic12_gpu_tile_alpha_group<16>(destination, source, pitch, alpha, pageY, word, vector, 3);
}

// False means no writes occurred; the caller can use its existing two passes.
static bool epic12_gpu_tile_alpha_page(void* atlasTiled,const uint32_t* source,int slot,
 Epic12GpuAlphaPage& alpha,int pitch=8192)
{
 if(!epic12_tile_alpha_enabled || !epic12_alpha_vector_enabled ||
    ((size_t)source&15) || (pitch&3))return false;
 Epic12GpuAlphaVector vector;
 alpha.invalidate_cache();
 epic12_gpu_tile_slot_init();
 const unsigned *slotOffsets = epic12_tile_slot_block_offset[slot];
 for(int by=0;by<4;++by) {
  int y = by * 32;
  const uint32_t *srcRow = source + y * pitch;
  for(int bx=0;bx<4;++bx) {
   int x = bx * 32;
   unsigned offset = slotOffsets[(by << 2) | bx];
   epic12_gpu_tile_alpha_block((unsigned char*)atlasTiled+offset,srcRow+x,
    pitch,alpha,y,bx,vector);
  }
 }
#if defined(EPIC12_GPU_ALPHA_PROFILE)
 epic12_gpu_alpha_stats.builtPixels+=128*128;
#endif
 return true;
}

// Private cached buffers, once at renderer initialization. The later GPU
// pixel self-test also exercises the actual WC upload and texture sampling.
static bool epic12_gpu_tile_alpha_selftest()
{
 epic12_tile_alpha_enabled=false;
 unsigned char* allocation=(unsigned char*)malloc(4096+256+127);
 uint32_t* sourceAllocation=(uint32_t*)malloc((32*136+4)*4);
 if(!allocation || !sourceAllocation) {free(allocation);free(sourceAllocation);return false;}
 uint32_t* destination=(uint32_t*)(((size_t)allocation+127)&~(size_t)127);
 uint32_t* source=(uint32_t*)(((size_t)sourceAllocation+15)&~(size_t)15);
 Epic12GpuAlphaVector vector;Epic12GpuAlphaPage alpha;
 bool ok=true;
 for(int pass=0;pass<4 && ok;++pass) {
  for(int y=0;y<32;++y)for(int x=0;x<32;++x) {
   uint32_t v=(x*0x1f3d5b79U)^(y*0x9e3779b9U)^(pass*0x71da89c3U);
   if(pass==1)v&=~0x20000000U;if(pass==2)v|=0x20000000U;
   source[y*136+x]=v;
  }
  for(int i=0;i<1088;++i)destination[i]=0xc35a7d91;
  for(int y=0;y<128;++y)for(int w=0;w<4;++w)alpha.rows[y][w]=0xc35a7d91;
  for(int g=0;g<16;++g)for(int w=0;w<4;++w)alpha.groups[g][w]=0xc35a7d91;
  epic12_gpu_tile_alpha_block((unsigned char*)(destination+32),source,136,alpha,pass*32,pass,vector);
  uint32_t group=0;
  for(int y=0;y<32;++y) {
   uint32_t mask=0;
   for(int x=0;x<32;++x) {
#if defined(_XBOX)
    unsigned offset=XGAddress2DTiledOffset(x,y,32,4);
#else
    unsigned offset=epic12_gpu_tile_address(x,y,32)/4;
#endif
    if(destination[32+offset]!=source[y*136+x])ok=false;
    mask=(mask<<1)|((source[y*136+x]>>29)&1);
   }
   if(alpha.rows[pass*32+y][pass]!=mask)ok=false;
   group=(y&7)?group|mask:mask;
   if((y&7)==7 && alpha.groups[pass*4+y/8][pass]!=group)ok=false;
  }
  for(int y=0;y<128;++y)for(int w=0;w<4;++w)
   if((w!=pass || y/32!=pass) && alpha.rows[y][w]!=0xc35a7d91)ok=false;
  for(int g=0;g<16;++g)for(int w=0;w<4;++w)
   if((w!=pass || g/4!=pass) && alpha.groups[g][w]!=0xc35a7d91)ok=false;
  for(int i=0;i<32;++i)if(destination[i]!=0xc35a7d91 || destination[1056+i]!=0xc35a7d91)ok=false;
 }
 free(allocation);free(sourceAllocation);epic12_tile_alpha_enabled=ok;return ok;
}
#undef EPIC12_TILE_ALPHA_INLINE
#endif
#endif
