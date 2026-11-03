// Exact EPIC12 bit-29 masks from cached, big-endian source pixels.
// No GPU memory reads, texture changes or approximate transparency tests.
#ifndef FBNEO_EPIC12_GPU_ALPHA_VMX_H
#define FBNEO_EPIC12_GPU_ALPHA_VMX_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#if defined(_XBOX)
#include <vectorintrinsics.h>
#define EPIC12_ALPHA_VECTOR 1
#define EPIC12_ALPHA_ALIGN __declspec(align(16))
#define EPIC12_ALPHA_INLINE __forceinline
#elif defined(EPIC12_GPU_ALPHA_VECTOR_TEST)
#define EPIC12_ALPHA_VECTOR 1
#define EPIC12_ALPHA_ALIGN __attribute__((aligned(16)))
#define EPIC12_ALPHA_INLINE inline __attribute__((always_inline))
#endif

#if defined(EPIC12_ALPHA_VECTOR)
// Constants are numeric big-endian words, including the byte permute selectors.
EPIC12_ALPHA_ALIGN static const uint32_t epic12_alpha_gather8[4]={
 0x0004080c,0x1014181c,0x0004080c,0x1014181c};
EPIC12_ALPHA_ALIGN static const uint32_t epic12_alpha_gather16[4]={
 0x00010203,0x04050607,0x10111213,0x14151617};
EPIC12_ALPHA_ALIGN static const uint32_t epic12_alpha_bit29[4]={
 0x20202020,0x20202020,0x20202020,0x20202020};
EPIC12_ALPHA_ALIGN static const uint32_t epic12_alpha_rotate[4]={
 0x02010007,0x06050403,0x02010007,0x06050403};
EPIC12_ALPHA_ALIGN static const uint32_t epic12_alpha_pack[4]={
 0x00081018,0x00081018,0x00081018,0x00081018};

#if defined(_XBOX)
static bool epic12_alpha_vector_enabled=false; // enabled only after startup test
#else
static bool epic12_alpha_vector_enabled=true;
#endif

struct Epic12GpuAlphaVector {
 __vector4 gather8,gather16,bit29,rotate,pack;
 Epic12GpuAlphaVector() {
  gather8=__lvx(epic12_alpha_gather8,0);gather16=__lvx(epic12_alpha_gather16,0);
  bit29=__lvx(epic12_alpha_bit29,0);rotate=__lvx(epic12_alpha_rotate,0);
  pack=__lvx(epic12_alpha_pack,0);
 }
  EPIC12_ALPHA_INLINE __vector4 half(__vector4 v0,__vector4 v1,__vector4 v2,__vector4 v3) const {
   __vector4 a=__vperm(v0,v1,gather8);
   __vector4 b=__vperm(v2,v3,gather8);
  __vector4 bits=__vrlb(__vand(__vperm(a,b,gather16),bit29),rotate);
  // Byte 0 collects pixels 0..7, byte 8 collects pixels 8..15.
  // Only those two bytes are used: rotations crossing group edges are ignored.
  bits=__vor(bits,__vsldoi(bits,bits,1));
  bits=__vor(bits,__vsldoi(bits,bits,2));
   return __vor(bits,__vsldoi(bits,bits,4));
  }
  EPIC12_ALPHA_INLINE __vector4 half(const uint32_t* source) const {
   return half(__lvx(source,0),__lvx(source,16),__lvx(source,32),__lvx(source,48));
  }
 EPIC12_ALPHA_INLINE void row(const uint32_t* source,uint32_t* masks) const {
  for(int word=0;word<4;++word) {
   __vector4 value=__vperm(half(source+word*32),half(source+word*32+16),pack);
   // Replicate the final word into every lane: stvewx selects its lane from
   // the destination address, which need only be aligned to four bytes.
   __stvewx(value,masks+word,0);
  }
 }
};

// Cached private memory, once per renderer initialization. No gameplay frames
// are sampled here. Failure keeps the original scalar builder available.
static bool epic12_gpu_alpha_vector_selftest()
{
 epic12_alpha_vector_enabled=false;
 unsigned char* allocation=(unsigned char*)malloc(128*4+31);
 if(!allocation)return false;
 uint32_t* source=(uint32_t*)(((size_t)allocation+15)&~(size_t)15);
 Epic12GpuAlphaVector vector;
 uint32_t output[8];unsigned random=0x53d6a109;
 bool ok=true;
 for(unsigned pass=0;pass<162 && ok;++pass) {
  for(unsigned x=0;x<128;++x) {
   random^=random<<13;random^=random>>17;random^=random<<5;
   source[x]=random&~0x20000000u;
   if(pass==128 || (pass<128 && x==pass) || (pass>129 && (random&1)))source[x]|=0x20000000u;
  }
  // Test all four word-store alignments and guards, not just lane zero.
  for(unsigned offset=0;offset<4 && ok;++offset) {
   for(unsigned i=0;i<8;++i)output[i]=0xc35a7d91;
   vector.row(source,output+offset);
   for(unsigned word=0;word<4;++word) {
    uint32_t expected=0;
    for(unsigned x=0;x<32;++x)expected=(expected<<1)|((source[word*32+x]>>29)&1);
    if(output[offset+word]!=expected)ok=false;
   }
   for(unsigned i=0;i<8;++i)if((i<offset || i>=offset+4) && output[i]!=0xc35a7d91)ok=false;
  }
 }
 free(allocation);epic12_alpha_vector_enabled=ok;return ok;
}
#endif
#undef EPIC12_ALPHA_ALIGN
#undef EPIC12_ALPHA_INLINE
#endif
