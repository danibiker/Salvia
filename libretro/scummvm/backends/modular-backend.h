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

#ifndef BACKENDS_MODULAR_BACKEND_H
#define BACKENDS_MODULAR_BACKEND_H

#include "backends/base-backend.h"

class GraphicsManager;
class MixerManager;

/**
 * Base classes for modular backends.
 *
 * They wrap most functions to their manager equivalent, but not
 * all OSystem functions are implemented here.
 *
 * A backend derivated from these classes, will need to implement
 * these functions on its own:
 *   OSystem::pollEvent()
 *   OSystem::createMutex()
 *   OSystem::getMillis()
 *   OSystem::delayMillis()
 *   OSystem::getTimeAndDate()
 *   OSystem::quit()
 *
 * And, it should also initialize all the managers variables
 * declared in this class, or override their related functions.
 */
class ModularGraphicsBackend : virtual public BaseBackend {
public:
	ModularGraphicsBackend();
	virtual ~ModularGraphicsBackend();

	/** @name Features */
	//@{

	bool hasFeature(Feature f) override;
	void setFeatureState(Feature f, bool enable) override;
	bool getFeatureState(Feature f) override;

	//@}

	/** @name Graphics */
	//@{

	GraphicsManager *getGraphicsManager() { return _graphicsManager; }
	const GraphicsMode *getSupportedGraphicsModes() const override;
	int getDefaultGraphicsMode() const override;
	bool setGraphicsMode(int mode, uint flags = kGfxModeNoFlags) override;
	int getGraphicsMode() const override;
#if defined(USE_IMGUI)
	void setImGuiCallbacks(const ImGuiCallbacks &callbacks) override final;
	void *getImGuiTexture(const Graphics::Surface &image, const byte *palette, int palCount) override final;
	void freeImGuiTexture(void *texture) override final;
#endif
	bool setShader(const Common::Path &name) override;
	const GraphicsMode *getSupportedStretchModes() const override;
	int getDefaultStretchMode() const override;
	bool setStretchMode(int mode) override;
	int getStretchMode() const override;
	bool setRotationMode(Common::RotationMode rotation) override;
	uint getDefaultScaler() const override;
	uint getDefaultScaleFactor() const override;
	using BaseBackend::setScaler;
	bool setScaler(uint mode, int factor) override;
	uint getScaler() const override;
	uint getScaleFactor() const override;
#ifdef USE_RGB_COLOR
	Graphics::PixelFormat getScreenFormat() const override;
	Common::List<Graphics::PixelFormat> getSupportedFormats() const override;
#endif
	void initSize(uint width, uint height, const Graphics::PixelFormat *format = NULL) override;
	void initSizeHint(const Graphics::ModeList &modes) override;
	int getScreenChangeID() const override;

	void beginGFXTransaction() override;
	OSystem::TransactionError endGFXTransaction() override;

	int16 getHeight() override;
	int16 getWidth() override;
	PaletteManager *getPaletteManager() override;
	void copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) override;
	Graphics::Surface *lockScreen() override;
	void unlockScreen() override;
	void fillScreen(uint32 col) override;
	void fillScreen(const Common::Rect &r, uint32 col) override;
	void updateScreen() override;
	void presentBuffer() override;
	void setShakePos(int shakeXOffset, int shakeYOffset) override;
	void setFocusRectangle(const Common::Rect& rect) override;
	void clearFocusRectangle() override;

	void showOverlay(bool inGUI) override;
	void hideOverlay() override;
	bool isOverlayVisible() const override;
	Graphics::PixelFormat getOverlayFormat() const override;
	void clearOverlay() override;
	void grabOverlay(Graphics::Surface &surface) override;
	void copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) override;
	int16 getOverlayHeight() const override;
	int16 getOverlayWidth() const override;
	Common::Rect getSafeOverlayArea(int16 *width, int16 *height) const override;

	float getHiDPIScreenFactor() const override;

	bool showMouse(bool visible) override;
	void warpMouse(int x, int y) override;
	void setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor, const Graphics::PixelFormat *format, const byte *mask, frac_t scaleX, frac_t scaleY) override;
	void setCursorPalette(const byte *colors, uint start, uint num) override;
	bool lockMouse(bool lock) override;

	//@}

	/** @name Miscellaneous */
	//@{

	void displayMessageOnOSD(const Common::U32String &msg) override;
	void displayActivityIconOnOSD(const Graphics::Surface *icon) override;

	void saveScreenshot() override;

	//@}

protected:
	/** @name Managers variables */
	//@{

	GraphicsManager *_graphicsManager;

	//@}
};

class ModularMixerBackend : virtual public BaseBackend {
public:
	ModularMixerBackend();
	virtual ~ModularMixerBackend();

	/** @name Sound */
	//@{

	virtual MixerManager *getMixerManager();
	Audio::Mixer *getMixer() override;

	//@}

protected:
	/** @name Managers variables */
	//@{

	MixerManager *_mixerManager;

	//@}
};

#endif
