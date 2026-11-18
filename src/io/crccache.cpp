#include "crccache.h"

#include <io/dirutil.h>
#include <utils/logger.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _XBOX
	#include <xtl.h>
#else
	#include <windows.h>
#endif

CrcCache::CrcCache(const std::string& file) : cacheFile(file), dirty(false) {
	load();
}

CrcCache::~CrcCache() {
	save();
}

std::string CrcCache::makeKey(const std::string& path) {
	std::string key(path);
	for (std::size_t i = 0; i < key.size(); i++) {
		if (key[i] == '/') key[i] = '\\';
		key[i] = (char)tolower((unsigned char)key[i]);
	}
	return key;
}

/* Tamano y fecha de ultima escritura sin abrir el fichero. */
bool CrcCache::fileStamp(const std::string& path, uint64_t& size, uint64_t& mtime) {
	WIN32_FILE_ATTRIBUTE_DATA fad;
	if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad))
		return false;
	if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		return false;
	size  = ((uint64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
	mtime = ((uint64_t)fad.ftLastWriteTime.dwHighDateTime << 32) | fad.ftLastWriteTime.dwLowDateTime;
	return true;
}

/* Lee un entero sin signo de 64 bits en decimal y avanza el puntero. */
static bool parseU64(const char*& p, uint64_t& out) {
	if (*p < '0' || *p > '9') return false;
	uint64_t v = 0;
	while (*p >= '0' && *p <= '9') {
		v = v * 10 + (uint64_t)(*p - '0');
		p++;
	}
	out = v;
	return true;
}

void CrcCache::load() {
	entries.clear();
	FILE* f = fopen(cacheFile.c_str(), "rb");
	if (!f) return;   // aun no hay cache: se creara al guardar

	char line[2048];
	int malas = 0;
	while (fgets(line, sizeof(line), f)) {
		std::size_t len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			line[--len] = '\0';
		if (len == 0) continue;

		const char* p = line;
		uint64_t size, mtime, crc;
		if (!parseU64(p, size)  || *p++ != '|' ||
		    !parseU64(p, mtime) || *p++ != '|' ||
		    !parseU64(p, crc)   || *p++ != '|' ||
		    *p == '\0' || crc > 0xFFFFFFFFULL) {
			malas++;
			continue;
		}

		Entry e;
		e.size = size;
		e.mtime = mtime;
		e.crc = (uint32_t)crc;
		e.path = p;
		entries[makeKey(e.path)] = e;
	}
	fclose(f);

	if (malas > 0) {
		LOG_DEBUG("CrcCache: %d lineas no validas en %s; se descartan\n", malas, cacheFile.c_str());
		dirty = true;   // reescribir la cache sin ellas
	}
}

bool CrcCache::getCrc(const std::string& path, uint32_t& crc) {
	crc = 0;
	uint64_t size, mtime;
	if (!fileStamp(path, size, mtime))
		return false;

	const std::string key = makeKey(path);
	std::map<std::string, Entry>::iterator it = entries.find(key);
	if (it != entries.end() && it->second.size == size && it->second.mtime == mtime) {
		crc = it->second.crc;
		return true;
	}

	bool ok = false;
	uint32_t c = dirutil::computeCrcFromFile(path, &ok);
	if (!ok)
		return false;

	Entry e;
	e.size = size;
	e.mtime = mtime;
	e.crc = c;
	e.path = path;
	entries[key] = e;
	dirty = true;
	crc = c;
	return true;
}

bool CrcCache::save() {
	if (!dirty) return true;

	/* Se escribe en un temporal y se sustituye al final, para no dejar una
	 * cache a medias si algo falla a mitad. */
	const std::string tmp = cacheFile + ".tmp";
	FILE* f = fopen(tmp.c_str(), "wb");
	if (!f) {
		LOG_DEBUG("CrcCache: no se puede escribir %s\n", tmp.c_str());
		return false;
	}

	bool ok = true;
	for (std::map<std::string, Entry>::const_iterator it = entries.begin(); it != entries.end(); ++it) {
		const Entry& e = it->second;
		if (fprintf(f, "%I64u|%I64u|%lu|%s\r\n", e.size, e.mtime, (unsigned long)e.crc, e.path.c_str()) < 0) {
			ok = false;
			break;
		}
	}
	if (fclose(f) != 0) ok = false;

	if (!ok) {
		remove(tmp.c_str());
		LOG_DEBUG("CrcCache: error escribiendo %s\n", tmp.c_str());
		return false;
	}

	remove(cacheFile.c_str());
	if (rename(tmp.c_str(), cacheFile.c_str()) != 0) {
		LOG_DEBUG("CrcCache: no se puede renombrar %s a %s\n", tmp.c_str(), cacheFile.c_str());
		return false;
	}
	dirty = false;
	return true;
}
