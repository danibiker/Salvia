// Row-strided rectangle copy out of CPU VRAM into a locked Xenos texture: one
// batch's bounding box, which is what the compositor hands the input texture
// every batch.
//
// `width` bytes from each of `rows` source rows, whose pitch is 8192 words
// (32 KB) in VRAM, are written to rows whose pitch is the driver's texture
// pitch. Neither side is contiguous and no row is longer than 2 KB, so one
// memcpy() per row hands the copy to a routine that sizes itself for a single
// long run of bytes: it cannot know that the next source line is 32 KB away
// and that the copy will never come back for the lines in between. This helper
// does know, so it fetches the source row a few rows ahead while copying this
// one.
//
// The destination is a locked texture, and on Xenon that is GPU write-combined
// memory. epic12_gpu_tile.h writes the same memory and states the rule for it:
// "all writes are aligned and sequential ... Never read it or use unaligned
// stores." The first version of this helper kept the unaligned pair on the
// store side -- legal, and in use, for the cached memory epic12_gpu_untile.h
// writes -- and the console went back to the dashboard the moment a state
// exercised that branch. So the store decides the shape of the loop here: a
// store must land on a 16-byte boundary, the source is the side allowed to be
// off, and anything with no aligned-store form falls back to memcpy() a row at
// a time, which is what the caller did before this helper existed.
//
// The rectangle is exact in both directions: bytes of a destination row beyond
// `width` are never read or written, and the copy rounds nothing out to a
// vector or a cache line.
#ifndef FBNEO_EPIC12_GPU_REGION_H
#define FBNEO_EPIC12_GPU_REGION_H
#include <stddef.h>
#include <string.h>

#if defined(_XBOX)
#include <ppcintrinsics.h>
#include <vectorintrinsics.h>
#define EPIC12_REGION_VECTOR 1
#elif defined(EPIC12_GPU_REGION_VECTOR_TEST)
// The standalone test supplies byte-accurate vector load/store intrinsics and
// asserts the alignment and bounds rules the SDK intrinsics require.
#define EPIC12_REGION_VECTOR 1
#endif

// Rows fetched ahead of the row being copied. The source rows are 32 KB apart,
// so the distance is small in rows and large in bytes.
#define EPIC12_REGION_PREFETCH_ROWS 4

#if defined(EPIC12_REGION_VECTOR)
template<bool ALIGNED_IN>
static inline void epic12_gpu_copy_row_vector(unsigned char *out, const unsigned char *in, size_t body, size_t width)
{
	size_t x = 0;
	// 128-byte unroll (8 vectors = full Xenon cache line / Write-Combine block)
	for (; x + 127 < body; x += 128) {
		__vector4 v0, v1, v2, v3, v4, v5, v6, v7;
		if (ALIGNED_IN) {
			v0 = __lvx(in + x, 0);
			v1 = __lvx(in + x + 16, 0);
			v2 = __lvx(in + x + 32, 0);
			v3 = __lvx(in + x + 48, 0);
			v4 = __lvx(in + x + 64, 0);
			v5 = __lvx(in + x + 80, 0);
			v6 = __lvx(in + x + 96, 0);
			v7 = __lvx(in + x + 112, 0);
		} else {
			v0 = __loadunalignedvector((const unsigned int*)(in + x));
			v1 = __loadunalignedvector((const unsigned int*)(in + x + 16));
			v2 = __loadunalignedvector((const unsigned int*)(in + x + 32));
			v3 = __loadunalignedvector((const unsigned int*)(in + x + 48));
			v4 = __loadunalignedvector((const unsigned int*)(in + x + 64));
			v5 = __loadunalignedvector((const unsigned int*)(in + x + 80));
			v6 = __loadunalignedvector((const unsigned int*)(in + x + 96));
			v7 = __loadunalignedvector((const unsigned int*)(in + x + 112));
		}
		__stvx_volatile(v0, out + x, 0);
		__stvx_volatile(v1, out + x + 16, 0);
		__stvx_volatile(v2, out + x + 32, 0);
		__stvx_volatile(v3, out + x + 48, 0);
		__stvx_volatile(v4, out + x + 64, 0);
		__stvx_volatile(v5, out + x + 80, 0);
		__stvx_volatile(v6, out + x + 96, 0);
		__stvx_volatile(v7, out + x + 112, 0);
	}
	// 64-byte step
	for (; x + 63 < body; x += 64) {
		__vector4 v0, v1, v2, v3;
		if (ALIGNED_IN) {
			v0 = __lvx(in + x, 0);
			v1 = __lvx(in + x + 16, 0);
			v2 = __lvx(in + x + 32, 0);
			v3 = __lvx(in + x + 48, 0);
		} else {
			v0 = __loadunalignedvector((const unsigned int*)(in + x));
			v1 = __loadunalignedvector((const unsigned int*)(in + x + 16));
			v2 = __loadunalignedvector((const unsigned int*)(in + x + 32));
			v3 = __loadunalignedvector((const unsigned int*)(in + x + 48));
		}
		__stvx_volatile(v0, out + x, 0);
		__stvx_volatile(v1, out + x + 16, 0);
		__stvx_volatile(v2, out + x + 32, 0);
		__stvx_volatile(v3, out + x + 48, 0);
	}
	// 16-byte step
	for (; x < body; x += 16) {
		__vector4 v = ALIGNED_IN ? __lvx(in + x, 0) : __loadunalignedvector((const unsigned int*)(in + x));
		__stvx_volatile(v, out + x, 0);
	}
	// Tail words (32-bit dwords)
	for (; x + 3 < width; x += 4)
		*(volatile unsigned int*)(out + x) = *(const unsigned int*)(in + x);
	// Trailing bytes (0 to 3 bytes)
	for (; x < width; ++x)
		out[x] = in[x];
}
#endif

static void epic12_gpu_copy_rows(void *destination, size_t dstPitch,
	const void *source, size_t srcPitch, size_t width, size_t rows)
{
	if (!width || !rows) return;
	unsigned char *out = (unsigned char*)destination;
	const unsigned char *in = (const unsigned char*)source;
#if defined(EPIC12_REGION_VECTOR)
	// Both pitches are multiples of 16, so both sides keep their alignment
	// down the rectangle and the split is decided once, outside the row loop.
	// A destination that is not vector aligned, or a source that is not even
	// word aligned, has no aligned-store form at all.
	if ((((size_t)out & 15) == 0) && (((size_t)in & 3) == 0) &&
		(((dstPitch | srcPitch) & 15) == 0)) {
		const size_t body = (width >> 4) << 4;
		const bool aligned_in = (((size_t)in & 15) == 0);
		if (aligned_in) {
			for (size_t y = 0; y < rows; ++y, out += dstPitch, in += srcPitch) {
				if (y + EPIC12_REGION_PREFETCH_ROWS < rows) {
					// dcbt is a hint: it cannot fault and cannot change a value.
					const unsigned char *ahead = in + EPIC12_REGION_PREFETCH_ROWS * srcPitch;
					for (size_t x = 0; x < width; x += 128) __dcbt((int)x, ahead);
				}
				epic12_gpu_copy_row_vector<true>(out, in, body, width);
			}
		} else {
			for (size_t y = 0; y < rows; ++y, out += dstPitch, in += srcPitch) {
				if (y + EPIC12_REGION_PREFETCH_ROWS < rows) {
					// dcbt is a hint: it cannot fault and cannot change a value.
					const unsigned char *ahead = in + EPIC12_REGION_PREFETCH_ROWS * srcPitch;
					for (size_t x = 0; x < width; x += 128) __dcbt((int)x, ahead);
				}
				epic12_gpu_copy_row_vector<false>(out, in, body, width);
			}
		}
		return;
	}
#endif
	// No aligned-store form: the same copy, a row at a time, by the routine
	// that handles its own alignment.
	for (size_t y = 0; y < rows; ++y, out += dstPitch, in += srcPitch)
		memcpy(out, in, width);
}

#undef EPIC12_REGION_VECTOR
#endif
