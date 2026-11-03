// Fixed-format EPIC12 readback: Xenos 512x512 A8R8G8B8 to cached CPU VRAM.
// Preserve LockRect(READONLY)/UnlockRect around this helper: those calls own
// synchronization and cache coherency. The source points to the texture base.
#ifndef FBNEO_EPIC12_GPU_UNTILE_H
#define FBNEO_EPIC12_GPU_UNTILE_H
#include <stddef.h>
#include <stdlib.h>

#if defined(_XBOX)
#include <ppcintrinsics.h>
#include <vectorintrinsics.h>
#include <xgraphics.h>
#define EPIC12_UNTILE_VECTOR 1
#elif defined(EPIC12_GPU_UNTILE_VECTOR_TEST)
// The standalone test supplies byte-accurate vector load/store intrinsics.
#define EPIC12_UNTILE_VECTOR 1
#endif

static unsigned short epic12_untile_offsets[32][8];
static unsigned int epic12_untile_blocks[16][16];
static bool epic12_untile_ready;

#if !defined(_XBOX)
// Portable address model for tests. The console uses the SDK address API.
// Within a 32x32/32bpp block, four adjacent pixels stay together. The bank
// selector exchanges x bit 4 according to y bit 3.
static unsigned int epic12_untile_address(unsigned x,unsigned y,unsigned width)
{
	unsigned local=((x&3)<<2)|((y&1)<<4)|((x&12)<<3)|
		((((x>>4)^(y>>3))&1)<<7)|((y&30)<<7);
	return (((y>>5)*(width>>5)+(x>>5))<<12)+local;
}
#endif

static void epic12_gpu_untile_init()
{
	if(epic12_untile_ready) return;
	for(unsigned y=0;y<32;++y) for(unsigned x=0;x<8;++x) {
#if defined(_XBOX)
		epic12_untile_offsets[y][x]=(unsigned short)(XGAddress2DTiledOffset(x*4,y,32,4)*4);
#else
		epic12_untile_offsets[y][x]=(unsigned short)epic12_untile_address(x*4,y,32);
#endif
	}
	for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x) {
#if defined(_XBOX)
		epic12_untile_blocks[y][x]=XGAddress2DTiledOffset(x*32,y*32,512,4)*4;
#else
		epic12_untile_blocks[y][x]=epic12_untile_address(x*32,y*32,512);
#endif
	}
	epic12_untile_ready=true;
}

static void epic12_gpu_untile_prefetch(const unsigned char *source,int width,int height)
{
#if defined(_XBOX)
	// Each 128-byte line holds a 16x2 strip. Touch only lines containing a
	// requested pixel, including partial blocks: the caller may have locked
	// a subrectangle, so unused lines must not enter the read-only cache view.
	for(int y=0;y<height;y+=2) {
		__dcbt(epic12_untile_offsets[y][0],source);
		if(width>16) __dcbt(epic12_untile_offsets[y][4],source);
	}
#else
	(void)source; (void)width; (void)height;
#endif
}

// Source rectangle is [0,width) x [0,height), with 0 <= width,height <= 512.
// Destination defaults to 8192 UINT32 pixels per row. Only the requested rectangle
// is written, including when min_x or the cached allocation is not 16-byte
// aligned. Four-byte destination alignment is required.
static void epic12_gpu_untile_mask(const void *tiled,unsigned int *vram,
	int min_x,int min_y,int width,int height,int dstPitchWords=8192)
{
	if(width<=0 || height<=0) return;
	epic12_gpu_untile_init();
	const unsigned char *source=(const unsigned char*)tiled;
	const int columns=(width+31)>>5, rows=(height+31)>>5;
#if defined(EPIC12_UNTILE_VECTOR)
	// Loading an integer mask preserves byte order on the big-endian CPU.
#if defined(_XBOX)
	__declspec(align(16))
#else
	__attribute__((aligned(16)))
#endif
	static const unsigned int maskWords[4]={
		0x20f8f8f8,0x20f8f8f8,0x20f8f8f8,0x20f8f8f8};
	const __vector4 mask=__lvx(maskWords,0);
#endif
	epic12_gpu_untile_prefetch(source+epic12_untile_blocks[0][0],width<32?width:32,height<32?height:32);
	for(int by=0;by<rows;++by) for(int bx=0;bx<columns;++bx) {
		const unsigned char *block=source+epic12_untile_blocks[by][bx];
		const int tileWidth=width-bx*32<32?width-bx*32:32;
		const int tileHeight=height-by*32<32?height-by*32:32;
		// Source order remains tiled while destination rows have a large
		// stride. A 32x32 block and the address tables fit comfortably in L1.
		if(bx+1<columns) {
			int nextWidth=width-(bx+1)*32;
			epic12_gpu_untile_prefetch(source+epic12_untile_blocks[by][bx+1],nextWidth<32?nextWidth:32,tileHeight);
		} else if(by+1<rows) {
			int nextHeight=height-(by+1)*32;
			epic12_gpu_untile_prefetch(source+epic12_untile_blocks[by+1][0],width<32?width:32,nextHeight<32?nextHeight:32);
		}
		for(int y=0;y<tileHeight;++y) {
			unsigned int *out=vram+(min_y+by*32+y)*dstPitchWords+min_x+bx*32;
			const unsigned short *row_offsets=epic12_untile_offsets[y];
#if defined(EPIC12_UNTILE_VECTOR)
			const bool aligned=(((size_t)out)&15)==0;
			if(tileWidth==32) {
				const __vector4 v0=__vand(__lvx((const unsigned int*)(block+row_offsets[0]),0),mask);
				const __vector4 v1=__vand(__lvx((const unsigned int*)(block+row_offsets[1]),0),mask);
				const __vector4 v2=__vand(__lvx((const unsigned int*)(block+row_offsets[2]),0),mask);
				const __vector4 v3=__vand(__lvx((const unsigned int*)(block+row_offsets[3]),0),mask);
				const __vector4 v4=__vand(__lvx((const unsigned int*)(block+row_offsets[4]),0),mask);
				const __vector4 v5=__vand(__lvx((const unsigned int*)(block+row_offsets[5]),0),mask);
				const __vector4 v6=__vand(__lvx((const unsigned int*)(block+row_offsets[6]),0),mask);
				const __vector4 v7=__vand(__lvx((const unsigned int*)(block+row_offsets[7]),0),mask);
				if(aligned) {
					__stvx(v0,out,0);
					__stvx(v1,out+4,0);
					__stvx(v2,out+8,0);
					__stvx(v3,out+12,0);
					__stvx(v4,out+16,0);
					__stvx(v5,out+20,0);
					__stvx(v6,out+24,0);
					__stvx(v7,out+28,0);
				} else {
					__storeunalignedvector(v0,out);
					__storeunalignedvector(v1,out+4);
					__storeunalignedvector(v2,out+8);
					__storeunalignedvector(v3,out+12);
					__storeunalignedvector(v4,out+16);
					__storeunalignedvector(v5,out+20);
					__storeunalignedvector(v6,out+24);
					__storeunalignedvector(v7,out+28);
				}
			} else {
				int x=0;
				if(aligned) {
					for(;x+8<=tileWidth;x+=8) {
						const unsigned int *in0=(const unsigned int*)(block+row_offsets[x>>2]);
						const unsigned int *in1=(const unsigned int*)(block+row_offsets[(x>>2)+1]);
						const __vector4 v0=__vand(__lvx(in0,0),mask);
						const __vector4 v1=__vand(__lvx(in1,0),mask);
						__stvx(v0,out+x,0);
						__stvx(v1,out+x+4,0);
					}
					if(x+4<=tileWidth) {
						const unsigned int *in=(const unsigned int*)(block+row_offsets[x>>2]);
						const __vector4 value=__vand(__lvx(in,0),mask);
						__stvx(value,out+x,0);
						x+=4;
					}
				} else {
					for(;x+8<=tileWidth;x+=8) {
						const unsigned int *in0=(const unsigned int*)(block+row_offsets[x>>2]);
						const unsigned int *in1=(const unsigned int*)(block+row_offsets[(x>>2)+1]);
						const __vector4 v0=__vand(__lvx(in0,0),mask);
						const __vector4 v1=__vand(__lvx(in1,0),mask);
						__storeunalignedvector(v0,out+x);
						__storeunalignedvector(v1,out+x+4);
					}
					if(x+4<=tileWidth) {
						const unsigned int *in=(const unsigned int*)(block+row_offsets[x>>2]);
						const __vector4 value=__vand(__lvx(in,0),mask);
						__storeunalignedvector(value,out+x);
						x+=4;
					}
				}
				if(x<tileWidth) {
					const unsigned int *in=(const unsigned int*)(block+row_offsets[x>>2]);
					for(int tail=0;x<tileWidth;++x,++tail) out[x]=in[tail]&0x20f8f8f8;
				}
			}
#else
			int x=0;
			for(;x+4<=tileWidth;x+=4) {
				const unsigned int *in=(const unsigned int*)(block+row_offsets[x>>2]);
				out[x]=in[0]&0x20f8f8f8;
				out[x+1]=in[1]&0x20f8f8f8;
				out[x+2]=in[2]&0x20f8f8f8;
				out[x+3]=in[3]&0x20f8f8f8;
			}
			if(x<tileWidth) {
				const unsigned int *in=(const unsigned int*)(block+row_offsets[x>>2]);
				for(int tail=0;x<tileWidth;++x,++tail) out[x]=in[tail]&0x20f8f8f8;
			}
#endif
		}
	}
}

#if defined(_XBOX)
// Run once before enabling this transfer path. No GPU resources are touched.
// A failed allocation or mismatch lets the caller keep its previous readback.
static bool epic12_gpu_untile_selftest()
{
	const unsigned guard=0xc35a7d91;
	unsigned char *sourceAllocation=(unsigned char*)malloc(512*512*4+15);
	unsigned int *destination=(unsigned int*)malloc((520*515+8)*4);
	if(!sourceAllocation || !destination) {
		free(sourceAllocation); free(destination); return false;
	}
	unsigned int *source=(unsigned int*)(((size_t)sourceAllocation+15)&~(size_t)15);
	for(unsigned y=0;y<512;++y) for(unsigned x=0;x<512;++x)
		source[XGAddress2DTiledOffset(x,y,512,4)]=
			(x*0x1f3d5b79U)^(y*0x9e3779b9U)^0xabcdef01U;
	const int cases[][5]={
		{512,512,0,0,520},{511,509,1,2,520},{319,321,2,1,520},
		{33,31,3,2,520},{31,33,0,1,519},{5,3,1,1,517},
		{3,5,2,0,518},{1,1,3,2,519}};
	bool ok=true;
	for(unsigned c=0;c<sizeof(cases)/sizeof(cases[0]) && ok;++c) {
		const int width=cases[c][0],height=cases[c][1],dx=cases[c][2],dy=cases[c][3],pitch=cases[c][4];
		const int size=pitch*(height+dy+1)+8;
		for(int i=0;i<size;++i) destination[i]=guard;
		epic12_gpu_untile_mask(source,destination+4,dx,dy,width,height,pitch);
		for(int i=0;i<size && ok;++i) {
			int p=i-4,y=p/pitch-dy,x=p%pitch-dx;
			unsigned expected=guard;
			if(p>=0 && x>=0 && x<width && y>=0 && y<height)
				expected=((x*0x1f3d5b79U)^(y*0x9e3779b9U)^0xabcdef01U)&0x20f8f8f8;
			if(destination[i]!=expected) ok=false;
		}
	}
	free(sourceAllocation); free(destination);
	return ok;
}
#endif

#undef EPIC12_UNTILE_VECTOR
#endif
