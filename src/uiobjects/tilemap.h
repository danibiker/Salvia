#pragma once

#include <SDL.h>
#include <SDL_image.h>

#include <cmath>
#include <string>
#include <utils/logger.h>

#define NUM_PARTICLES 200

struct ColorRGB { Uint8 r, g, b; };

struct Particle {
	float x, y, speedY, phase;
	void reset(SDL_Surface* surface) {
		x = (float)(rand() % surface->w);
		//y = (float)(surface->h + (rand() % 50));
		y = (float)(rand() % surface->h);
		speedY = 0.2f + (static_cast<float>(rand()) / RAND_MAX) * 2.0f;
		phase = static_cast<float>(rand()) / RAND_MAX * 6.28318f;
	}
};

class TileMap {
public:
	TileMap(int tileX, int tileY, int tileW, int tileH)
		: tileX(tileX), tileY(tileY), tileW(tileW), tileH(tileH),
		  img(nullptr), tile(nullptr), speed(0.0f),
		  imageW(0), imageH(0), particlesInitiated(false)
	{
		particles = new std::vector<Particle>(NUM_PARTICLES);
		cachedBg = NULL;
	}

	~TileMap() {
		if (img)  SDL_FreeSurface(img);
		if (tile) SDL_FreeSurface(tile);
		delete particles;
		if (cachedBg) SDL_FreeSurface(cachedBg);
	}

	void incSpeed(){
		speed++;
	}

	void load(const std::string& imgpath) {
		img = IMG_Load(imgpath.c_str());
		if (img) {
			imageW = img->w;
			imageH = img->h;
			findTile(tileX, tileY);
		} else {
			LOG_ERROR("Couldn't load %s\n", imgpath.c_str());
		}
	}

	void findTile(int x, int y) {
		if (!img) {
			LOG_ERROR("Image surface is null...\n");
			return;
		}

		if (tile) SDL_FreeSurface(tile);

		tileX = x;
		tileY = y;

		tile = SDL_CreateRGBSurface(SDL_SWSURFACE, tileW, tileH, 16, 0, 0, 0, 0);

		SDL_Rect srcRect = { (Sint16)(tileX * tileW), (Sint16)(tileY * tileH),
		                     (Uint16)tileW, (Uint16)tileH };
		SDL_Rect dstRect = { 0, 0, (Uint16)tileW, (Uint16)tileH };

		SDL_BlitSurface(img, &srcRect, tile, &dstRect);
	}

	void draw(SDL_Surface* video_page) {
		if (!tile || !video_page) {
			LOG_ERROR("Some surface is null...\n");
			return;
		}

		SDL_Rect srcRect = { 0, 0, (Uint16)tileW, (Uint16)tileH };
		SDL_Rect dstRect;

		const int iSpeed = (int)speed;
		// Precompute modulo offsets so inner loop only does subtraction
		const int modX = iSpeed % tileW;
		const int modY = iSpeed % tileH;

		for (int y = -tileH; y < video_page->h + tileH; y += tileH) {
			dstRect.y = (Sint16)(y - ((y + modY) % tileH));
			for (int x = -tileW; x < video_page->w + tileW; x += tileW) {
				dstRect.x = (Sint16)(x - ((x + modX) % tileW));
				SDL_BlitSurface(tile, &srcRect, video_page, &dstRect);
			}
		}
	}

	void drawWaves(SDL_Surface* video_page) {

		static Uint32 lastUpdate = 0;
		const Uint32 INTERVAL_MS = 100; // 10 fps = 1000ms / 10

		const Uint32 now = SDL_GetTicks();
		if (now - lastUpdate < INTERVAL_MS){
			SDL_BlitSurface(cachedBg, NULL, video_page, NULL);
			return;
		}
		lastUpdate = now;

		if (!particlesInitiated) {
			for (std::size_t i = 0; i < particles->size(); ++i) {
				particles->at(i).reset(video_page);
			}
			particlesInitiated = true;
			cachedBg = SDL_DisplayFormat(video_page);
		}

		const float ticks = (float)SDL_GetTicks();
		const float time  = ticks * 0.0005f;

		// Background color with very slow transitions
		ColorRGB currentBG;
		currentBG.r = (Uint8)(15 + 10 * sinf(time * 0.04f));
		currentBG.g = (Uint8)(25 + 15 * sinf(time * 0.06f));
		currentBG.b = (Uint8)(60 + 25 * sinf(time * 0.02f));

		if (SDL_MUSTLOCK(video_page)) SDL_LockSurface(video_page);

		applyGradientMotionBlur(video_page, currentBG);

		// Draw star-dust particles (bluish white, faint)
		for (std::size_t i = 0; i < particles->size(); ++i) {
			auto& p = particles->at(i);
			p.y    -= p.speedY;
			p.phase += 0.015f;
			if (p.y < -5.0f) p.reset(video_page);
			const float flicker = (sinf(p.phase) + 1.0f) * 0.5f;
			blendPixel(video_page, (int)p.x, (int)p.y, 180, 220, 255, (Uint8)(100.0f * flicker));
		}

		// Silk bands same hue, varying brightness
		drawSilkBand(video_page, ticks,         0.003f, 85.0f, 0.00012f, 45, 4, currentBG, 1.3f);
		drawSilkBand(video_page, ticks * 0.8f,  0.005f, 55.0f, 0.00008f, 45, 4, currentBG, 2.8f);
		drawSilkBand(video_page, ticks * 1.4f,  0.008f, 35.0f, 0.00004f, 30, 2, currentBG, 5.5f);

		SDL_FreeSurface(cachedBg);
		cachedBg = SDL_DisplayFormat(video_page);

		if (SDL_MUSTLOCK(video_page)) SDL_UnlockSurface(video_page);
	}

private:
	int          imageW, imageH;
	SDL_Surface* img;
	SDL_Surface* tile;
	std::vector<Particle>* particles;
	bool particlesInitiated;
	SDL_Surface *cachedBg;
	int   tileX, tileY, tileW, tileH;
	float speed;

	// -----------------------------------------------------------------------
	// Inline pixel blend — no SDL_GetRGB / SDL_MapRGB overhead for hot path.
	// Assumes RGB565 (16-bit) surface, which matches SDL_CreateRGBSurface(…,16,…)
	// -----------------------------------------------------------------------
	inline void blendPixel(SDL_Surface* surface, int x, int y,
	                       Uint8 r, Uint8 g, Uint8 b, Uint8 alpha)
	{
		if ((unsigned)x >= (unsigned)surface->w ||
		    (unsigned)y >= (unsigned)surface->h) return;

		Uint16* pixels = (Uint16*)surface->pixels;
		Uint16& px     = pixels[y * (surface->pitch >> 1) + x];

		// Unpack RGB565
		Uint8 rb = (Uint8)((px >> 8) & 0xF8);
		Uint8 gb = (Uint8)((px >> 3) & 0xFC);
		Uint8 bb = (Uint8)((px << 3) & 0xF8);

		// Alpha blend
		Uint8 ia = 255 - alpha;
		Uint8 nr = (Uint8)((r * alpha + rb * ia) >> 8);
		Uint8 ng = (Uint8)((g * alpha + gb * ia) >> 8);
		Uint8 nb = (Uint8)((b * alpha + bb * ia) >> 8);

		// Pack RGB565
		px = (Uint16)(((nr & 0xF8) << 8) | ((ng & 0xFC) << 3) | (nb >> 3));
	}

	// -----------------------------------------------------------------------
	// Background gradient + motion-blur trail.
	// One SDL_MapRGB per row (was one per pixel before).
	// -----------------------------------------------------------------------
	void applyGradientMotionBlur(SDL_Surface* screen, ColorRGB topColor) {
		Uint16* pixels        = (Uint16*)screen->pixels;
		const int pitch16     = screen->pitch >> 1;
		const float invH      = 1.0f / screen->h;
		const float rBase     = (float)topColor.r;
		const float gBase     = (float)topColor.g;
		const float bBase     = (float)topColor.b;

		for (int y = 0; y < screen->h; y++) {
			const float grad = 1.0f - y * invH * 0.7f;
			const Uint8 tr   = (Uint8)(rBase * grad);
			const Uint8 tg   = (Uint8)(gBase * grad);
			const Uint8 tb   = (Uint8)(bBase * grad);

			Uint16* row = &pixels[y * pitch16];

			// Sample first pixel of the row for the blur trail
			const Uint16 raw = row[0];
			const Uint8  rb  = (Uint8)((raw >> 8) & 0xF8);
			const Uint8  gb  = (Uint8)((raw >> 3) & 0xFC);
			const Uint8  bb  = (Uint8)((raw << 3) & 0xF8);

			// 85/255 ≈ 33% new, 170/255 ≈ 67% old  — same ratio as before
			const Uint8 nr = (Uint8)((tr * 85 + rb * 170) >> 8);
			const Uint8 ng = (Uint8)((tg * 85 + gb * 170) >> 8);
			const Uint8 nb = (Uint8)((tb * 85 + bb * 170) >> 8);

			// Pre-pack the row colour once, then memset-style fill
			const Uint16 col16 = (Uint16)(((nr & 0xF8) << 8) |
			                              ((ng & 0xFC) << 3) |
			                              (nb >> 3));

			for (int x = 0; x < screen->w; x++) row[x] = col16;
		}
	}

	// -----------------------------------------------------------------------
	// Silk-band renderer.
	// Pre-computes the sin/cos lookup table per column to avoid redundant trig.
	// -----------------------------------------------------------------------
	void drawSilkBand(SDL_Surface* screen, float ticks, float freq, float amp,
	                  float spd, int strands, int thickness,
	                  ColorRGB base, float brightness)
	{
		const Uint8 r = (Uint8)std::min(255.0f, base.r * brightness);
		const Uint8 g = (Uint8)std::min(255.0f, base.g * brightness);
		const Uint8 b = (Uint8)std::min(255.0f, base.b * brightness);

		const float   halfH        = screen->h * 0.5f;
		const float   tickOffset   = ticks * spd;
		const int     halfThick    = thickness >> 1;
		const float   strandInvF   = 1.0f / (float)strands;
		const float   strandThickF = (float)thickness * 0.8f;

		for (int x = 0; x < screen->w; x++) {
			const float angle  = x * freq + tickOffset;
			const float sinA   = sinf(angle);
			const float yBase  = halfH + amp * sinA;

			for (int i = 0; i < strands; i++) {
				const float offsetAngle = angle + i * 0.12f;
				const int   yCurrent    = (int)(yBase + strandThickF * i * cosf(offsetAngle * 0.4f));
				const Uint8 alpha       = (Uint8)(6 + 18.0f * (1.0f - i * strandInvF));

				for (int h = -halfThick; h <= halfThick; h++) {
					const int py = yCurrent + h;
					if ((unsigned)py < (unsigned)screen->h)
						blendPixel(screen, x, py, r, g, b, alpha);
				}
			}
		}
	}
};