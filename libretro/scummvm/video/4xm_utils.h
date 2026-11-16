/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef VIDEO_4XM_UTILS_H
#define VIDEO_4XM_UTILS_H

#include "common/array.h"
#include "common/compression/huffman.h"
#include "common/debug.h"

namespace Video {
namespace FourXM {

struct RawDeltaReader {
	const byte *data;
	uint32 size;
	uint32 mode;
	uint32 wordBitsLeft;
	uint32 wordIndex;
	uint32 controlWord;
	uint32 pairIndex;
	uint32 byteIndex;
	uint32 nibbleBitsLeft;
	uint32 nibbleWordIndex;
	uint32 nibbleWord;
	uint32 pairOffset;
	uint32 byteOffset;
	uint32 signedByteOffset;
	uint32 nibbleOffset;
	uint32 signedByteIndex;
	bool separateSignedByteStream;

	RawDeltaReader() : data(nullptr), size(0), mode(0), wordBitsLeft(16), wordIndex(1), controlWord(0), pairIndex(0), byteIndex(0), nibbleBitsLeft(8), nibbleWordIndex(1), nibbleWord(0), pairOffset(0), byteOffset(0), signedByteOffset(0), nibbleOffset(0), signedByteIndex(0), separateSignedByteStream(false) {};
	RawDeltaReader(const byte *ptr, uint32 len);
	RawDeltaReader(const byte *ptr, uint32 len, uint32 streamBase, uint32 pairOff, uint32 byteOff,
				   uint32 signedByteOff, uint32 nibbleOff, uint32 controlOff);

	bool valid() const;
	uint32 readControl2();
	uint16 readPair();
	byte readByteIndex();
	int8 readSignedByte();
	uint32 readNibble();
};

template<typename HuffmanType>
HuffmanType loadStatistics(const byte *&huff, uint &offset) {
	Common::Array<uint32> freqs, symbols;

	uint8 freq_first = huff[offset++];
	do {
		uint8 freq_last = huff[offset++];
		if (freq_first <= freq_last) {
			for (auto idx = freq_first; idx <= freq_last; ++idx) {
				auto freq = huff[offset++];
				if (freq != 0) {
					freqs.push_back(freq);
					symbols.push_back(idx);
				}
			}
		}
		freq_first = huff[offset++];
	} while (freq_first != 0);

	freqs.push_back(1);
	symbols.push_back(256);

	return HuffmanType::fromFrequencies(freqs.size(), freqs.data(), symbols.data());
}

void idct(int16_t block[64], int shift = 6);
void buildRawMotionTables(int width, Common::Array<int> &fullOffsets, Common::Array<int> &expOffsets);
void copyRawBlock(uint16 *dst, const uint16 *src, int stride, int width, int height, uint32 mode, int add = 0);
void readRawCoefficients(RawDeltaReader &reader, int coeffs[3][64]);
void transformRawCoefficients(int coeff[64], int dst[64], int scaleCode, bool chroma);
void writeRawDctBlock(const int yBlock[64], const int cbBlock[64], const int crBlock[64],
					  uint16 *dst, int stride);

inline int readInt(int value, unsigned n) {
	if (n == 0)
		return 0;
	if ((value & (1 << (n - 1))) == 0)
		value += 1 - (1 << n);
	return value;
}

template<typename BitStream>
inline int readInt(BitStream &bs, size_t n) {
	return readInt(bs.getBits(n), n);
}

} // namespace FourXM
} // namespace Video

#endif // VIDEO_4XM_UTILS_H
