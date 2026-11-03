// Upload a cached 128x128 source page into a 2048-wide Xenos tiled atlas.
// The destination is GPU write-combined memory: all writes are aligned and
// sequential within each 32x32 block. Never read it or use unaligned stores.
#ifndef FBNEO_EPIC12_GPU_TILE_H
#define FBNEO_EPIC12_GPU_TILE_H
#include <stddef.h>
#include <stdlib.h>

#if defined(_XBOX)
#include <ppcintrinsics.h>
#include <vectorintrinsics.h>
#include <xgraphics.h>
#define EPIC12_TILE_VECTOR 1
#elif defined(EPIC12_GPU_TILE_VECTOR_TEST)
#define EPIC12_TILE_VECTOR 1
#endif

static unsigned epic12_tile_source_offsets[256];
static int epic12_tile_source_pitch;

#if !defined(_XBOX)
// Portable test model. Native address construction uses the SDK helper.
static unsigned epic12_gpu_tile_address(unsigned x,unsigned y,unsigned width)
{
	unsigned local=((x&3)<<2)|((y&1)<<4)|((x&12)<<3)|
		((((x>>4)^(y>>3))&1)<<7)|((y&30)<<7);
	return (((y>>5)*(width>>5)+(x>>5))<<12)+local;
}
#endif

static void epic12_gpu_tile_init(int sourcePitchWords)
{
	if(epic12_tile_source_pitch==sourcePitchWords) return;
	// Invert the 32x32 block permutation once. Four adjacent 32-bit texels
	// stay together, allowing each 16-byte destination chunk to be written
	// exactly once in increasing address order.
	for(unsigned y=0;y<32;++y) for(unsigned x=0;x<32;x+=4) {
#if defined(_XBOX)
		unsigned destination=XGAddress2DTiledOffset(x,y,32,4)*4;
#else
		unsigned destination=epic12_gpu_tile_address(x,y,32);
#endif
		epic12_tile_source_offsets[destination/16]=y*sourcePitchWords+x;
	}
	epic12_tile_source_pitch=sourcePitchWords;
}

#if defined(EPIC12_TILE_VECTOR)
template<bool aligned> static __vector4 epic12_gpu_tile_load(const unsigned int *source)
{
	// Only the source may use the unaligned pair, and it must be cached.
	if(aligned) return __lvx(source,0);
	return __loadunalignedvector(source);
}
#endif

template<bool aligned> static void epic12_gpu_tile_block(unsigned char *destination,
	const unsigned int *source)
{
	for(int chunk=0;chunk<256;chunk+=8) {
#if defined(_XBOX)
		// The following transaction reads another 16x2 source strip. Only
		// prefetch addresses actually belonging to this source block.
		if(chunk+8<256) {
			__dcbt(0,source+epic12_tile_source_offsets[chunk+8]);
			__dcbt(0,source+epic12_tile_source_offsets[chunk+9]);
		}
#endif
#if defined(EPIC12_TILE_VECTOR)
		// Eight consecutive 16-byte stores fill two 64-byte store/gather
		// buffers. Volatile stores prevent compiler reordering on WC memory.
		__vector4 v0=epic12_gpu_tile_load<aligned>(source+epic12_tile_source_offsets[chunk]);
		__vector4 v1=epic12_gpu_tile_load<aligned>(source+epic12_tile_source_offsets[chunk+1]);
		__vector4 v2=epic12_gpu_tile_load<aligned>(source+epic12_tile_source_offsets[chunk+2]);
		__vector4 v3=epic12_gpu_tile_load<aligned>(source+epic12_tile_source_offsets[chunk+3]);
		__vector4 v4=epic12_gpu_tile_load<aligned>(source+epic12_tile_source_offsets[chunk+4]);
		__vector4 v5=epic12_gpu_tile_load<aligned>(source+epic12_tile_source_offsets[chunk+5]);
		__vector4 v6=epic12_gpu_tile_load<aligned>(source+epic12_tile_source_offsets[chunk+6]);
		__vector4 v7=epic12_gpu_tile_load<aligned>(source+epic12_tile_source_offsets[chunk+7]);
		__stvx_volatile(v0,destination,0); __stvx_volatile(v1,destination,16);
		__stvx_volatile(v2,destination,32); __stvx_volatile(v3,destination,48);
		__stvx_volatile(v4,destination,64); __stvx_volatile(v5,destination,80);
		__stvx_volatile(v6,destination,96); __stvx_volatile(v7,destination,112);
#else
		for(int i=0;i<8;++i) {
			const unsigned int *in=source+epic12_tile_source_offsets[chunk+i];
			unsigned int *out=(unsigned int*)(destination+i*16);
			out[0]=in[0]; out[1]=in[1]; out[2]=in[2]; out[3]=in[3];
		}
#endif
		destination+=128;
	}
}

static unsigned epic12_tile_slot_block_offset[256][16];
static bool epic12_tile_slot_offsets_ready = false;

static void epic12_gpu_tile_slot_init()
{
	if(epic12_tile_slot_offsets_ready) return;
	for(unsigned slot=0;slot<256;++slot) {
		unsigned ax=(slot&15)*128, ay=(slot>>4)*128;
		for(unsigned by=0;by<4;++by) {
			for(unsigned bx=0;bx<4;++bx) {
				unsigned y=by*32, x=bx*32;
#if defined(_XBOX)
				epic12_tile_slot_block_offset[slot][(by<<2)|bx]=XGAddress2DTiledOffset(ax+x,ay+y,2048,4)*4;
#else
				epic12_tile_slot_block_offset[slot][(by<<2)|bx]=epic12_gpu_tile_address(ax+x,ay+y,2048);
#endif
			}
		}
	}
	epic12_tile_slot_offsets_ready = true;
}

// sourcePage points at a 128x128 page's top-left pixel in ordinary cached RAM.
// atlasTiled points at level zero of a tiled A8R8G8B8 texture of width 2048.
// Use slots 0..127 for height 1024 or 0..255 for height 2048. The caller retains
// ownership of LockRect/UnlockRect and must supply a 16-byte-aligned destination.
static void epic12_gpu_tile_page(void *atlasTiled,const unsigned int *sourcePage,
	int slot,int sourcePitchWords=8192)
{
	epic12_gpu_tile_init(sourcePitchWords);
	epic12_gpu_tile_slot_init();
	const unsigned *slotOffsets = epic12_tile_slot_block_offset[slot];
	bool aligned=(((size_t)sourcePage&15)==0) && ((sourcePitchWords&3)==0);
	for(int by=0;by<4;++by) {
		int y = by * 32;
		const unsigned int *srcRow = sourcePage + y * sourcePitchWords;
		for(int bx=0;bx<4;++bx) {
			int x = bx * 32;
			unsigned offset = slotOffsets[(by << 2) | bx];
			unsigned char *destination = (unsigned char*)atlasTiled + offset;
			const unsigned int *source = srcRow + x;
			if(aligned) epic12_gpu_tile_block<true>(destination,source);
			else epic12_gpu_tile_block<false>(destination,source);
		}
	}
}

#if defined(_XBOX)
// Check the real vector instructions with small private cached allocations.
// GPU pixel validation additionally checks WC uploads and texture sampling.
// No atlas resources, game VRAM, or cache-page tags are changed here.
static bool epic12_gpu_tile_selftest()
{
	unsigned char *destinationAllocation=(unsigned char*)malloc(4096+256+127);
	unsigned int *sourceAllocation=(unsigned int*)malloc((32*137+8)*4);
	if(!destinationAllocation || !sourceAllocation) {
		free(destinationAllocation);free(sourceAllocation);return false;
	}
	unsigned *destination=(unsigned*)(((size_t)destinationAllocation+127)&~(size_t)127);
	bool ok=true;
	for(int pass=0;pass<4 && ok;++pass) {
		int pitch=132+pass;
		unsigned *source=sourceAllocation+pass;
		for(int y=0;y<32;++y) for(int x=0;x<32;++x)
			source[y*pitch+x]=(x*0x1f3d5b79U)^(y*0x9e3779b9U)^0xabcdef01U;
		for(int i=0;i<32+1024+32;++i) destination[i]=0xc35a7d91;
		epic12_gpu_tile_init(pitch);
		if((((size_t)source&15)==0) && ((pitch&3)==0))
			epic12_gpu_tile_block<true>((unsigned char*)(destination+32),source);
		else epic12_gpu_tile_block<false>((unsigned char*)(destination+32),source);
		for(unsigned y=0;y<32 && ok;++y) for(unsigned x=0;x<32;++x) {
			unsigned expected=(x*0x1f3d5b79U)^(y*0x9e3779b9U)^0xabcdef01U;
			if(destination[32+XGAddress2DTiledOffset(x,y,32,4)]!=expected) {ok=false;break;}
		}
		for(int i=0;i<32;++i)
			if(destination[i]!=0xc35a7d91 || destination[1056+i]!=0xc35a7d91) ok=false;
	}
	free(destinationAllocation);free(sourceAllocation);
	return ok;
}
#endif

#undef EPIC12_TILE_VECTOR
#endif
