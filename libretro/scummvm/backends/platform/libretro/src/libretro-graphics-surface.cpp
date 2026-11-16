/* Copyright (C) 2023 Giovanni Cascione <ing.cascione@gmail.com>
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

#include "graphics/colormasks.h"
#include "graphics/palette.h"
#include "graphics/managed_surface.h"

#include "gui/message.h"
#include "gui/gui-manager.h"

#include "backends/platform/libretro/include/libretro-defs.h"
#include "backends/platform/libretro/include/libretro-core.h"
#include "backends/platform/libretro/include/libretro-os.h"
#include "backends/platform/libretro/include/libretro-timer.h"
#include "backends/platform/libretro/include/libretro-graphics-surface.h"

#include <features/features_cpu.h>

LibretroGraphics::LibretroGraphics() : _cursorPaletteEnabled(false),
	_cursorKeyColor(0),
	_cursorScaleX(0),
	_cursorScaleY(0),
	_screenUpdatePending(false),
	_gamePalette(256),
	_cursorPalette(256),
	_screenChangeID(1 << (sizeof(int) * 8 - 2)) {}

LibretroGraphics::~LibretroGraphics() {
	_gameScreen.free();
	_overlay.free();
	_cursor.free();
	_screen.free();
}

Common::List<Graphics::PixelFormat> LibretroGraphics::getSupportedFormats() const {
	Common::List<Graphics::PixelFormat> result;

#ifdef SCUMM_LITTLE_ENDIAN
	/* RGBA8888 */
	result.push_back(Graphics::PixelFormat(4, 8, 8, 8, 8, 24, 16, 8, 0));
#else
	/* ABGR8888 */
	result.push_back(Graphics::PixelFormat(4, 8, 8, 8, 8, 0, 8, 16, 24));
#endif
	/* RGB565 - overlay */
	result.push_back(Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));

	/* RGB555 - fmtowns */
	result.push_back(Graphics::PixelFormat(2, 5, 5, 5, 1, 10, 5, 0, 15));

	/* Palette - most games */
	result.push_back(Graphics::PixelFormat::createFormatCLUT8());

	return result;
}

const OSystem::GraphicsMode *LibretroGraphics::getSupportedGraphicsModes() const {
	static const OSystem::GraphicsMode s_noGraphicsModes[] = {{0, 0, 0}};
	return s_noGraphicsModes;
}

void LibretroGraphics::initSize(uint width, uint height, const Graphics::PixelFormat *format) {
	Graphics::PixelFormat actFormat = format ? *format : Graphics::PixelFormat::createFormatCLUT8();
	bool force_gui_redraw = false;
	/* Override for ScummVM Launcher */
	if (nullptr == ConfMan.getActiveDomain()) {
		/* 0 w/h is used to notify libretro gui res settings is changed */
		force_gui_redraw = (width == 0);
		width = retro_setting_get_gui_res_w();
		height = retro_setting_get_gui_res_h();
	}
	/* no need to update now libretro gui res settings changes if not in ScummVM launcher */
	if (! width)
		return;

	if (_gameScreen.w != width || _gameScreen.h != height || _gameScreen.format != actFormat)
		_gameScreen.create(width, height, actFormat);

	if (_overlay.w != width || _overlay.h != height)
		_overlay.create(width, height, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));

	if (getWindowWidth() != width || getWindowHeight() != height)
		_screen.create(width, height, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));

	handleResize(width, height);
	recalculateDisplayAreas();
	retro_set_size(width, height);
	LIBRETRO_G_SYSTEM->refreshRetroSettings();
	++_screenChangeID;

	if (force_gui_redraw)
		g_gui.checkScreenChange();
}

int16 LibretroGraphics::getHeight() const {
	return _gameScreen.h;
}

int16 LibretroGraphics::getWidth() const {
	return _gameScreen.w;
}

Graphics::PixelFormat LibretroGraphics::getScreenFormat() const {
	return _gameScreen.format;
}

void LibretroGraphics::copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) {
	_gameScreen.copyRectToSurface(buf, pitch, x, y, w, h);
}

void LibretroGraphics::updateScreen() {
	_screenUpdatePending = true;
	dynamic_cast<LibretroTimerManager *>(LIBRETRO_G_SYSTEM->getTimerManager())->checkThread(THREAD_SWITCH_UPDATE);
}

/* Time spent in realUpdateScreen(), read by the performance log in libretro-core.cpp. */
uint64 retro_perf_blit_usec = 0;

/* Fast copy of a full frame into the RGB565 output surface when no scaling is
 * needed (the usual case: _screen is created with the size of _gameScreen and
 * _overlay).  ManagedSurface::blitFrom() handles every case through a generic
 * per-pixel loop (format checks, alpha tests, scale counters for every pixel),
 * which on the in-order Xbox 360 CPU costs tens of milliseconds for a
 * 1280x720 launcher frame.  Returns false when the generic path is needed. */
static bool fastBlitToScreen(Graphics::ManagedSurface &dst, const Graphics::Surface &src, const Graphics::Palette &palette) {
	if (src.w != dst.w || src.h != dst.h || dst.format.bytesPerPixel != 2)
		return false;

	byte *dstRow = (byte *)dst.getPixels();
	const byte *srcRow = (const byte *)src.getPixels();
	const int w = src.w;
	const int h = src.h;

	if (src.format == dst.format) {
		const uint rowBytes = w * 2;
		if ((uint)src.pitch == rowBytes && (uint)dst.pitch == rowBytes) {
			memcpy(dstRow, srcRow, rowBytes * h);
		} else {
			for (int y = 0; y < h; ++y, dstRow += dst.pitch, srcRow += src.pitch)
				memcpy(dstRow, srcRow, rowBytes);
		}
		return true;
	}

	if (src.format.isCLUT8()) {
		uint16 lut[256];
		const uint palSize = palette.size();
		for (uint i = 0; i < 256; ++i) {
			byte r = 0, g = 0, b = 0;
			if (i < palSize)
				palette.get(i, r, g, b);
			lut[i] = (uint16)dst.format.RGBToColor(r, g, b);
		}
		for (int y = 0; y < h; ++y, dstRow += dst.pitch, srcRow += src.pitch) {
			uint16 *d = (uint16 *)dstRow;
			const byte *sp = srcRow;
			int x = 0;
			for (; x + 4 <= w; x += 4) {
				d[x]     = lut[sp[x]];
				d[x + 1] = lut[sp[x + 1]];
				d[x + 2] = lut[sp[x + 2]];
				d[x + 3] = lut[sp[x + 3]];
			}
			for (; x < w; ++x)
				d[x] = lut[sp[x]];
		}
		return true;
	}

	if (dst.format == Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0) &&
	    src.format.bytesPerPixel == 4 && src.format.rLoss == 0 && src.format.gLoss == 0 && src.format.bLoss == 0) {
		/* 32 bpp with 8-bit components (TinyGL engines) -> RGB565, opaque copy
		 * like the generic path does for fully opaque pixels. */
		const uint rS = src.format.rShift, gS = src.format.gShift, bS = src.format.bShift;
		for (int y = 0; y < h; ++y, dstRow += dst.pitch, srcRow += src.pitch) {
			uint16 *d = (uint16 *)dstRow;
			const uint32 *sp = (const uint32 *)srcRow;
			for (int x = 0; x < w; ++x) {
				const uint32 c = sp[x];
				d[x] = (uint16)((((c >> rS) & 0xF8) << 8) | (((c >> gS) & 0xFC) << 3) | (((c >> bS) & 0xFF) >> 3));
			}
		}
		return true;
	}

	return false;
}

void LibretroGraphics::realUpdateScreen(void) {
	const uint64 t0 = cpu_features_get_time_usec();
	const Graphics::Surface &srcSurface = _overlayVisible ? _overlay : _gameScreen;

	if (srcSurface.w && srcSurface.h) {
#ifndef LIBRETRO_NO_FAST_BLIT /* define it to compare against the generic path */
		if (!fastBlitToScreen(_screen, srcSurface, _gamePalette))
#endif
			_screen.blitFrom(srcSurface, Common::Rect(srcSurface.w, srcSurface.h), Common::Rect(_screen.w, _screen.h), &_gamePalette);
	}

	if (_cursorVisible && _cursor.w && _cursor.h) {
		Common::Point topLeft(_cursorX - _cursorHotspotXScaled, _cursorY - _cursorHotspotYScaled);
		_screen.transBlitFrom(_cursor, Common::Rect(_cursor.w, _cursor.h), Common::Rect(topLeft, topLeft + Common::Point(_cursorWidthScaled, _cursorHeightScaled)),  _cursorKeyColor, false, 0xff,  _cursorPaletteEnabled ? &_cursorPalette : &_gamePalette);
	}
	_screenUpdatePending = false;
	retro_perf_blit_usec += cpu_features_get_time_usec() - t0;
}

void LibretroGraphics::clearOverlay() {
	_overlay.fillRect(Common::Rect(_overlay.w, _overlay.h), 0);
}

void LibretroGraphics::grabOverlay(Graphics::Surface &surface) const {
	surface.copyFrom(_overlay);
}

void LibretroGraphics::copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) {
	_overlay.copyRectToSurface(buf, pitch, x, y, w, h);
}

int16 LibretroGraphics::getOverlayHeight() const {
	return _overlay.h;
}

int16 LibretroGraphics::getOverlayWidth() const {
	return _overlay.w;
}

Graphics::PixelFormat LibretroGraphics::getOverlayFormat() const {
	return _overlay.format;
}

void LibretroGraphics::warpMouse(int x, int y) {
	LIBRETRO_G_SYSTEM->_mouseX = x;
	LIBRETRO_G_SYSTEM->_mouseY = y;
	WindowedGraphicsManager::warpMouse(x, y);
}

void LibretroGraphics::overrideCursorScaling() {
	const float cursorScaleFactorX = (_cursorScaleX == 0 || ! _overlayVisible) ? 1.0f : (float)getWindowHeight() / 200 * _cursorScaleX; /* hard coded as base resolution 320x200 is hard coded upstream */
	const float cursorScaleFactorY = (_cursorScaleY == 0 || ! _overlayVisible) ? 1.0f : (float)getWindowHeight() / 200 * _cursorScaleY; /* hard coded as base resolution 320x200 is hard coded upstream */

	_cursorHotspotXScaled = _cursorHotspotX * cursorScaleFactorX;
	_cursorWidthScaled    = _cursor.w * cursorScaleFactorX;

	_cursorHotspotYScaled = _cursorHotspotY * cursorScaleFactorY;
	_cursorHeightScaled   = _cursor.h * cursorScaleFactorY;
}

void LibretroGraphics::setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor, const Graphics::PixelFormat *format, const byte *mask, frac_t scaleX, frac_t scaleY) {
	if (!buf || !w || !h)
		return;

	const Graphics::PixelFormat mformat = format ? *format : Graphics::PixelFormat::createFormatCLUT8();

	if (_cursor.w != w || _cursor.h != h || _cursor.format != mformat)
		_cursor.create(w, h, mformat);

	_cursor.copyRectToSurface(buf, _cursor.pitch, 0, 0, w, h);

	_cursorHotspotX = hotspotX;
	_cursorHotspotY = hotspotY;
	_cursorKeyColor = keycolor;
	_cursorScaleX = fracToDouble(scaleX);
	_cursorScaleY = fracToDouble(scaleY);

	overrideCursorScaling();
}


OSystem::TransactionError LibretroGraphics::endGFXTransaction() {
	overrideCursorScaling();
	return OSystem::TransactionError::kTransactionSuccess;
}

void LibretroGraphics::handleResizeImpl(const int width, const int height) {
	overrideCursorScaling();
}

void LibretroGraphics::setCursorPalette(const byte *colors, uint start, uint num) {
	_cursorPalette.set(colors, start, num);
	_cursorPaletteEnabled = true;
}

bool LibretroGraphics::isOverlayInGUI(void) {
	return _overlayInGUI;
}

const Graphics::ManagedSurface *LibretroGraphics::getScreen() {
	return &_screen;
}

void LibretroGraphics::setPalette(const byte *colors, uint start, uint num) {
	_gamePalette.set(colors, start, num);
}

void LibretroGraphics::grabPalette(byte *colors, uint start, uint num) const {
	_gamePalette.grab(colors, start, num);
}

bool LibretroGraphics::hasFeature(OSystem::Feature f) const {
	return (f == OSystem::kFeatureCursorPalette) ||
#ifdef SCUMMVM_NEON
	       (f == OSystem::kFeatureCpuNEON) ||
#endif
	       (f == OSystem::kFeatureCursorAlpha);
}

void LibretroGraphics::setFeatureState(OSystem::Feature f, bool enable) {
	if (f == OSystem::kFeatureCursorPalette)
		_cursorPaletteEnabled = enable;
}

bool LibretroGraphics::getFeatureState(OSystem::Feature f) const {
	return (f == OSystem::kFeatureCursorPalette) ? _cursorPaletteEnabled : false;
}

void LibretroGraphics::setMousePosition(int x, int y) {
	WindowedGraphicsManager::setMousePosition(x, y);
}

void LibretroGraphics::displayMessageOnOSD(const Common::U32String &msg) {
	// Display the message for 3 seconds
	GUI::TimedMessageDialog dialog(msg, 3000);
	dialog.runModal();
}

int LibretroGraphics::getScreenChangeID() const {
	return _screenChangeID;
}
