/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/tile-cache.h>

#include <mgba-util/memory.h>

void mTileCacheInit(struct mTileCache* cache) {
	// TODO: Reconfigurable cache for space savings
	cache->cache = NULL;
	cache->config = mTileCacheConfigurationFillShouldStore(0);
	cache->status = NULL;
	cache->globalPaletteVersion = NULL;
	cache->palette = NULL;
}

static void _freeCache(struct mTileCache* cache) {
	unsigned size = 1 << mTileCacheSystemInfoGetPaletteCount(cache->sysConfig);
	unsigned tiles = mTileCacheSystemInfoGetMaxTiles(cache->sysConfig);
	if (cache->cache) {
		mappedMemoryFree(cache->cache, 8 * 8 * sizeof(mColor) * tiles * size);
		cache->cache = NULL;
	}
	if (cache->status) {
		mappedMemoryFree(cache->status, tiles * size * sizeof(*cache->status));
		cache->status = NULL;
	}
	free(cache->globalPaletteVersion);
	cache->globalPaletteVersion = NULL;
	free(cache->palette);
	cache->palette = NULL;
}

static void _redoCacheSize(struct mTileCache* cache) {
	unsigned size;
	unsigned bpp;
	unsigned tiles;
	if (!mTileCacheConfigurationIsShouldStore(cache->config)) {
		return;
	}
	size = mTileCacheSystemInfoGetPaletteCount(cache->sysConfig);
	bpp = mTileCacheSystemInfoGetPaletteBPP(cache->sysConfig);
	cache->bpp = bpp;
	bpp = 1 << (1 << bpp);
	size = 1 << size;
	cache->entriesPerTile = size;
	tiles = mTileCacheSystemInfoGetMaxTiles(cache->sysConfig);
	cache->cache = anonymousMemoryMap(8 * 8 * sizeof(mColor) * tiles * size);
	cache->status = anonymousMemoryMap(tiles * size * sizeof(*cache->status));
	cache->globalPaletteVersion = calloc(size, sizeof(*cache->globalPaletteVersion));
	cache->palette = calloc(size * bpp, sizeof(*cache->palette));
}

void mTileCacheConfigure(struct mTileCache* cache, mTileCacheConfiguration config) {
	if (cache->config == config) {
		return;
	}
	_freeCache(cache);
	cache->config = config;
	_redoCacheSize(cache);
}

void mTileCacheConfigureSystem(struct mTileCache* cache, mTileCacheSystemInfo config, uint32_t tileBase, uint32_t paletteBase) {
	_freeCache(cache);
	cache->sysConfig = config;
	cache->tileBase = tileBase;
	cache->paletteBase = paletteBase;
	_redoCacheSize(cache);
}

void mTileCacheDeinit(struct mTileCache* cache) {
	_freeCache(cache);
}

void mTileCacheWriteVRAM(struct mTileCache* cache, uint32_t address) {
	unsigned bpp;
	unsigned count;
	size_t i;
	if (address < cache->tileBase) {
		return;
	}
	address -= cache->tileBase;
	bpp = cache->bpp + 3;
	count = cache->entriesPerTile;
	address >>= bpp;
	if (address >= mTileCacheSystemInfoGetMaxTiles(cache->sysConfig)) {
		return;
	}
	for (i = 0; i < count; ++i) {
		cache->status[address * count + i].vramClean = 0;
		++cache->status[address * count + i].vramVersion;
	}
}

void mTileCacheWritePalette(struct mTileCache* cache, uint32_t entry, mColor color) {
	unsigned maxEntry;
	if (entry < cache->paletteBase) {
		return;
	}
	entry -= cache->paletteBase;
	maxEntry = (1 << (1 << cache->bpp)) * cache->entriesPerTile;
	if (entry >= maxEntry) {
		return;
	}
	cache->palette[entry] = color;
	entry >>= (1 << mTileCacheSystemInfoGetPaletteBPP(cache->sysConfig));
	++cache->globalPaletteVersion[entry];
}

static void _regenerateTile4(struct mTileCache* cache, mColor* tile, unsigned tileId, unsigned paletteId) {
	mColor* palette;
	int i;
	uint8_t* start = (uint8_t*) &cache->vram[tileId << 3];
	paletteId <<= 2;
	palette = &cache->palette[paletteId];
	for (i = 0; i < 8; ++i) {
		int pixel;
		uint8_t tileDataLower = start[0];
		uint8_t tileDataUpper = start[1];
		start += 2;
		pixel = ((tileDataUpper & 128) >> 6) | ((tileDataLower & 128) >> 7);
		tile[0] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = ((tileDataUpper & 64) >> 5) | ((tileDataLower & 64) >> 6);
		tile[1] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = ((tileDataUpper & 32) >> 4) | ((tileDataLower & 32) >> 5);
		tile[2] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = ((tileDataUpper & 16) >> 3) | ((tileDataLower & 16) >> 4);
		tile[3] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = ((tileDataUpper & 8) >> 2) | ((tileDataLower & 8) >> 3);
		tile[4] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = ((tileDataUpper & 4) >> 1) | ((tileDataLower & 4) >> 2);
		tile[5] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = (tileDataUpper & 2) | ((tileDataLower & 2) >> 1);
		tile[6] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
		tile[7] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		tile += 8;
	}
}

static void _regenerateTile16(struct mTileCache* cache, mColor* tile, unsigned tileId, unsigned paletteId) {
	mColor* palette;
	int i;
	uint32_t* start = (uint32_t*) &cache->vram[tileId << 4];
	paletteId <<= 4;
	palette = &cache->palette[paletteId];
	for (i = 0; i < 8; ++i) {
		int pixel;
		uint32_t line = *start;
		++start;
		pixel = line & 0xF;
		tile[0] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = (line >> 4) & 0xF;
		tile[1] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = (line >> 8) & 0xF;
		tile[2] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = (line >> 12) & 0xF;
		tile[3] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = (line >> 16) & 0xF;
		tile[4] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = (line >> 20) & 0xF;
		tile[5] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = (line >> 24) & 0xF;
		tile[6] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = (line >> 28) & 0xF;
		tile[7] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		tile += 8;
	}
}

static void _regenerateTile256(struct mTileCache* cache, mColor* tile, unsigned tileId, unsigned paletteId) {
	mColor* palette;
	int i;
	uint32_t* start = (uint32_t*) &cache->vram[tileId << 5];
	paletteId <<= 8;
	palette = &cache->palette[paletteId];
	for (i = 0; i < 8; ++i) {
		int pixel;
		uint32_t line = *start;
		++start;
		pixel = line & 0xFF;
		tile[0] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = (line >> 8) & 0xFF;
		tile[1] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = (line >> 16) & 0xFF;
		tile[2] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = (line >> 24) & 0xFF;
		tile[3] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];

		line = *start;
		++start;
		pixel = line & 0xFF;
		tile[4] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = (line >> 8) & 0xFF;
		tile[5] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = (line >> 16) & 0xFF;
		tile[6] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		pixel = (line >> 24) & 0xFF;
		tile[7] = pixel ? palette[pixel] | 0xFF000000 : palette[pixel];
		tile += 8;
	}
}

static inline mColor* _tileLookup(struct mTileCache* cache, unsigned tileId, unsigned paletteId) {
	if (mTileCacheConfigurationIsShouldStore(cache->config)) {
		unsigned tiles = mTileCacheSystemInfoGetMaxTiles(cache->sysConfig);
		mASSERT(tileId < tiles);
		mASSERT_DEBUG(paletteId < 1U << mTileCacheSystemInfoGetPaletteCount(cache->sysConfig));
		return &cache->cache[(tileId + paletteId * tiles) << 6];
	} else {
		return cache->temporaryTile;
	}
}

const mColor* mTileCacheGetTile(struct mTileCache* cache, unsigned tileId, unsigned paletteId) {
	unsigned count = cache->entriesPerTile;
	unsigned bpp = cache->bpp;
	struct mTileCacheEntry* status = &cache->status[tileId * count + paletteId];
	struct mTileCacheEntry desiredStatus = {
		/* .paletteVersion */ cache->globalPaletteVersion[paletteId],
		/* .vramVersion */ status->vramVersion,
		/* .vramClean */ 1,
		/* .paletteId */ paletteId,
	};
	mColor* tile = _tileLookup(cache, tileId, paletteId);
	if (!mTileCacheConfigurationIsShouldStore(cache->config) || memcmp(status, &desiredStatus, sizeof(*status))) {
		switch (bpp) {
		case 0:
			return NULL;
		case 1:
			_regenerateTile4(cache, tile, tileId, paletteId);
			break;
		case 2:
			_regenerateTile16(cache, tile, tileId, paletteId);
			break;
		case 3:
			_regenerateTile256(cache, tile, tileId, paletteId);
			break;
		}
		*status = desiredStatus;
	}
	return tile;
}

const mColor* mTileCacheGetTileIfDirty(struct mTileCache* cache, struct mTileCacheEntry* entry, unsigned tileId, unsigned paletteId) {
	unsigned count = cache->entriesPerTile;
	unsigned bpp = cache->bpp;
	struct mTileCacheEntry* status = &cache->status[tileId * count + paletteId];
	struct mTileCacheEntry desiredStatus = {
		/* .paletteVersion */ cache->globalPaletteVersion[paletteId],
		/* .vramVersion */ status->vramVersion,
		/* .vramClean */ 1,
		/* .paletteId */ paletteId,
	};
	mColor* tile = NULL;
	if (memcmp(status, &desiredStatus, sizeof(*status))) {
		tile = _tileLookup(cache, tileId, paletteId);
		switch (bpp) {
		case 0:
			return NULL;
		case 1:
			_regenerateTile4(cache, tile, tileId, paletteId);
			break;
		case 2:
			_regenerateTile16(cache, tile, tileId, paletteId);
			break;
		case 3:
			_regenerateTile256(cache, tile, tileId, paletteId);
			break;
		}
		*status = desiredStatus;
	}
	if (memcmp(status, &entry[paletteId], sizeof(*status))) {
		tile = _tileLookup(cache, tileId, paletteId);
		entry[paletteId] = *status;
	}
	return tile;
}

const mColor* mTileCacheGetPalette(struct mTileCache* cache, unsigned paletteId) {
	return &cache->palette[paletteId << (1 << cache->bpp)];
}

const uint16_t* mTileCacheGetVRAM(struct mTileCache* cache, unsigned tileId) {
	unsigned tiles = mTileCacheSystemInfoGetMaxTiles(cache->sysConfig);
	if (tileId >= tiles) {
		return NULL;
	}
	return &cache->vram[tileId << (cache->bpp + 2)];
}
