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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCUMM_INSANE_MORTIMER_MOVIE_H
#define SCUMM_INSANE_MORTIMER_MOVIE_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

namespace Common {
class SeekableReadStream;
}

namespace Graphics {
struct Surface;
}

namespace Scumm {

class SmushDeltaBlocksDecoder;

struct MortimerSprite {
	int x, y, width, height;
	Common::Array<byte> pixels;
	// Codec 44 uses skipped pixels for transparency and relative palette colors.
	Common::Array<byte> mask;

	void draw(Graphics::Surface &screen, int dstX, int dstY, bool centered = false, const Common::Rect *bounds = nullptr, int scale = 1) const;
};

struct MortimerSound {
	MortimerSound() : frame(0), rate(11025), volume(127), balance(0), music(false), speech(true) {}

	uint frame;
	uint rate;
	byte volume;
	int8 balance;
	bool music;
	bool speech;
	Common::Array<byte> samples;
	Common::Array<byte> script;
};

struct MortimerMovieContext {
	MortimerMovieContext() : girl(false), riddle(-1), conditions() {}

	enum Condition { kAtGate, kHasStones, kHostilesRemain, kAnimalsRemain, kDamaged, kOutOfGuesses, kConditionCount };
	bool girl;
	int riddle;
	bool conditions[kConditionCount];
	bool matches(uint code) const;
};

bool loadMortimerSprites(Common::SeekableReadStream &stream, Common::Array<MortimerSprite> &sprites, byte *palette = nullptr);
bool loadMortimerSound(Common::SeekableReadStream &stream, MortimerSound &sound);

// Mortimer's SMUSH movie reader. Gameplay supplies the IACT and SKIP rules.
class MortimerMovie {
public:
	enum { kScreenWidth = 640, kScreenHeight = 480 };
	struct Caption {
		int16 x, y;
		uint16 id;
	};

	MortimerMovie();
	virtual ~MortimerMovie();
	bool load(Common::SeekableReadStream &stream, bool doubled = true, bool continuation = false);
	virtual void reset();
	// Suppress compositing while seeking to a frame that replaces the image.
	virtual bool advance(bool renderFrame = true);
	void setBackground(const byte *pixels);

	const byte *pixels() const { return _pixels.begin(); }
	const byte *palette() const { return _palette; }
	uint frame() const { return _nextFrame ? _nextFrame - 1 : 0; }
	uint frameCount() const { return _frames.size(); }
	bool started() const { return _nextFrame != 0; }
	uint frameRate() const { return _frameRate; }
	const Common::Array<MortimerSound> &sounds() const { return _sounds; }
	const Common::Array<Caption> &captions() const { return _captions; }
	bool soundEnabled(uint index) const { return _soundEnabled[index]; }
	const Common::Array<byte> &cachedMovie() const { return _cachedMovie; }
	const Common::String &errorMessage() const { return _error; }

protected:
	virtual void handleAction(const byte *data, uint32 size);
	virtual bool skipObject(uint16 a, uint16 b) const { return false; }
	virtual int objectScale(byte codec) const;
	void render();

private:
	struct Frame {
		uint32 offset, size;
	};
	struct Object {
	Object() : offset(0), size(0), skipA(0xffff), skipB(0), x(0), y(0), width(0), height(0), scale(1), codec(0), transparent(false), remap() {}

		uint32 offset, size;
		uint16 skipA, skipB;
		int x, y, width, height, scale;
		byte codec;
		bool transparent;
		byte remap[256];
		Common::Array<byte> pixels;
	};

	bool fail(const char *message);
	void handlePalette(const byte *data, uint32 size);
	bool decodeObject(Object &object);

	Common::Array<byte> _data, _pixels, _previous, _scratch, _background;
	Common::Array<Frame> _frames;
	Common::Array<Object> _objects;
	Common::Array<MortimerSound> _sounds;
	Common::Array<uint32> _soundOffsets;
	Common::Array<bool> _soundEnabled;
	Common::Array<byte> _cachedMovie;
	uint _cacheBlock;
	Common::Array<Caption> _captions;
	Object _storedObject;
	SmushDeltaBlocksDecoder *_decoder;
	Common::String _error;
	byte _palette[768], _initialPalette[768], _remap[256];
	int16 _paletteDelta[768];
	int32 _shiftedPalette[768];
	int _decoderWidth, _decoderHeight;
	bool _doubled, _initialDoubled, _storeNext;
	uint _nextFrame, _frameRate;
};

class MortimerCutscene : public MortimerMovie {
public:
	explicit MortimerCutscene(bool girl = false);
	void reset() override;
	MortimerMovieContext &context() { return _context; }
	const MortimerMovieContext &context() const { return _context; }

protected:
	void handleAction(const byte *data, uint32 size) override;
	bool skipObject(uint16 a, uint16 b) const override;
	bool toggle(uint16 id) const { return id > 0 && id <= 1024 && _toggles[id - 1]; }
	void setToggle(uint16 id, bool value) { if (id > 0 && id <= 1024) _toggles[id - 1] = value; }

private:
	MortimerMovieContext _context;
	bool _toggles[1024];
};

} // End of namespace Scumm

#endif
