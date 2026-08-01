#include <cheats/rdbreader.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace {

const int      MP_MAX_DEPTH = 32;
const long     RDB_MAX_SIZE = 64L * 1024 * 1024;   // tope al cargar el .rdb en memoria

// Cursor de lectura sobre el .rdb ya cargado en memoria (con control de limites).
struct Cur { const uint8_t* p; const uint8_t* end; };

bool cU8(Cur& c, uint8_t& v) {
	if (c.p >= c.end) return false;
	v = *c.p++;
	return true;
}

// Entero de n bytes en BIG-endian (enteros rmsgpack del .rdb).
bool cBE(Cur& c, int n, uint64_t& out) {
	if ((c.end - c.p) < n) return false;
	uint64_t v = 0;
	for (int i = 0; i < n; ++i) v = (v << 8) | (uint64_t)(*c.p++);
	out = v;
	return true;
}

bool cAdvance(Cur& c, uint64_t n) {
	if ((uint64_t)(c.end - c.p) < n) return false;
	c.p += (size_t)n;
	return true;
}

bool mpSkip(Cur& c, int depth);

// Cabecera de mapa -> numero de pares. Marca isNil si el valor es el centinela nil
// (0xc0) que cierra la lista de registros. false si no es mapa ni nil.
bool mpMapHeader(Cur& c, uint32_t& count, bool& isNil) {
	isNil = false;
	uint8_t t;
	if (!cU8(c, t)) return false;
	if (t == 0xc0) { isNil = true; return true; }                          // nil (fin de registros)
	if ((t & 0xf0) == 0x80) { count = (uint32_t)(t & 0x0f); return true; } // fixmap
	uint64_t n;
	if (t == 0xde) { if (!cBE(c, 2, n)) return false; count = (uint32_t)n; return true; } // map16
	if (t == 0xdf) { if (!cBE(c, 4, n)) return false; count = (uint32_t)n; return true; } // map32
	return false;
}

// String: devuelve puntero+longitud dentro del buffer (sin copiar).
bool mpStrRef(Cur& c, const uint8_t*& s, uint32_t& len) {
	uint8_t t;
	if (!cU8(c, t)) return false;
	uint64_t n;
	if ((t & 0xe0) == 0xa0)      n = (uint64_t)(t & 0x1f);                  // fixstr
	else if (t == 0xd9) { if (!cBE(c, 1, n)) return false; }               // str8
	else if (t == 0xda) { if (!cBE(c, 2, n)) return false; }               // str16
	else if (t == 0xdb) { if (!cBE(c, 4, n)) return false; }               // str32
	else return false;
	if ((uint64_t)(c.end - c.p) < n) return false;
	s = c.p; len = (uint32_t)n; c.p += (size_t)n;
	return true;
}

// Binary: devuelve puntero+longitud dentro del buffer (sin copiar).
bool mpBinRef(Cur& c, const uint8_t*& b, uint32_t& len) {
	uint8_t t;
	if (!cU8(c, t)) return false;
	uint64_t n;
	if (t == 0xc4)      { if (!cBE(c, 1, n)) return false; }                // bin8
	else if (t == 0xc5) { if (!cBE(c, 2, n)) return false; }               // bin16
	else if (t == 0xc6) { if (!cBE(c, 4, n)) return false; }               // bin32
	else return false;
	if ((uint64_t)(c.end - c.p) < n) return false;
	b = c.p; len = (uint32_t)n; c.p += (size_t)n;
	return true;
}

// Salta cualquier valor msgpack (recursivo, profundidad acotada).
bool mpSkip(Cur& c, int depth) {
	if (depth <= 0) return false;
	uint8_t t;
	if (!cU8(c, t)) return false;

	if (t < 0x80)           return true;                                    // positive fixint
	if (t >= 0xe0)          return true;                                    // negative fixint
	if ((t & 0xe0) == 0xa0) return cAdvance(c, (uint64_t)(t & 0x1f));       // fixstr
	if ((t & 0xf0) == 0x80) {                                               // fixmap -> 2n valores
		uint32_t n = (uint32_t)(t & 0x0f);
		for (uint32_t i = 0; i < 2 * n; ++i) if (!mpSkip(c, depth - 1)) return false;
		return true;
	}
	if ((t & 0xf0) == 0x90) {                                               // fixarray -> n valores
		uint32_t n = (uint32_t)(t & 0x0f);
		for (uint32_t i = 0; i < n; ++i) if (!mpSkip(c, depth - 1)) return false;
		return true;
	}

	uint64_t n = 0;
	switch (t) {
		case 0xc0: case 0xc2: case 0xc3: return true;                       // nil / false / true
		case 0xcc: case 0xd0: return cAdvance(c, 1);                        // uint8 / int8
		case 0xcd: case 0xd1: return cAdvance(c, 2);                        // uint16 / int16
		case 0xce: case 0xd2: case 0xca: return cAdvance(c, 4);             // uint32 / int32 / float32
		case 0xcf: case 0xd3: case 0xcb: return cAdvance(c, 8);             // uint64 / int64 / float64
		case 0xd9: case 0xc4: if (!cBE(c, 1, n)) return false; return cAdvance(c, n); // str8 / bin8
		case 0xda: case 0xc5: if (!cBE(c, 2, n)) return false; return cAdvance(c, n); // str16 / bin16
		case 0xdb: case 0xc6: if (!cBE(c, 4, n)) return false; return cAdvance(c, n); // str32 / bin32
		case 0xde: if (!cBE(c, 2, n)) return false; for (uint64_t i = 0; i < 2 * n; ++i) if (!mpSkip(c, depth - 1)) return false; return true; // map16
		case 0xdf: if (!cBE(c, 4, n)) return false; for (uint64_t i = 0; i < 2 * n; ++i) if (!mpSkip(c, depth - 1)) return false; return true; // map32
		case 0xdc: if (!cBE(c, 2, n)) return false; for (uint64_t i = 0; i < n; ++i) if (!mpSkip(c, depth - 1)) return false; return true;     // array16
		case 0xdd: if (!cBE(c, 4, n)) return false; for (uint64_t i = 0; i < n; ++i) if (!mpSkip(c, depth - 1)) return false; return true;     // array32
	}
	return false;
}

// Normaliza para comparar: minusculas, sin el contenido de ()/[], solo alfanumerico
// con espacios colapsados (equivalente a Constant::limpiarNombreJuego, autonomo).
std::string cleanNameImpl(const std::string& s) {
	std::string out;
	int  depth = 0;
	bool lastSpace = true;
	for (std::size_t i = 0; i < s.size(); ++i) {
		char ch = s[i];
		if (ch == '(' || ch == '[') { depth++; continue; }
		if (ch == ')' || ch == ']') { if (depth > 0) depth--; continue; }
		if (depth > 0) continue;
		if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
		bool alnum = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9');
		if (alnum) { out += ch; lastSpace = false; }
		else if (!lastSpace) { out += ' '; lastSpace = true; }
	}
	while (!out.empty() && out[out.size() - 1] == ' ') out.erase(out.size() - 1);
	return out;
}

} // namespace

bool RdbReader::findNameByCrc(const std::string& rdbPath, uint32_t crc, std::string& outName) {
	FILE* f = std::fopen(rdbPath.c_str(), "rb");
	if (!f) return false;

	// Cargamos el .rdb entero en memoria (los oficiales rondan 1-12 MB; tope 64 MB).
	if (std::fseek(f, 0, SEEK_END) != 0) { std::fclose(f); return false; }
	long sz = std::ftell(f);
	if (sz <= 16 || sz > RDB_MAX_SIZE) { std::fclose(f); return false; }
	if (std::fseek(f, 0, SEEK_SET) != 0) { std::fclose(f); return false; }

	uint8_t* buf = (uint8_t*)std::malloc((size_t)sz);
	if (!buf) { std::fclose(f); return false; }
	bool readOk = std::fread(buf, 1, (size_t)sz, f) == (size_t)sz;
	std::fclose(f);
	if (!readOk) { std::free(buf); return false; }

	bool ok = false;

	do {
		// Cabecera: "RARCHDB\0" (8) + metadata_offset (u64 BE). Los registros van del
		// byte 16 hasta metadata_offset; NO hay indices (los .rdb oficiales solo traen
		// registros + un trailer de metadata), asi que escaneamos linealmente.
		if (std::memcmp(buf, "RARCHDB", 7) != 0) break;
		uint64_t metaOff = 0;
		for (int i = 8; i < 16; ++i) metaOff = (metaOff << 8) | (uint64_t)buf[i];
		if (metaOff > (uint64_t)sz) metaOff = (uint64_t)sz;

		// Clave: los 4 bytes del CRC en big-endian (como se guarda el campo binario).
		uint8_t key[4];
		key[0] = (uint8_t)(crc >> 24);
		key[1] = (uint8_t)(crc >> 16);
		key[2] = (uint8_t)(crc >> 8);
		key[3] = (uint8_t)(crc);

		Cur c;
		c.p   = buf + 16;
		c.end = buf + metaOff;

		while (c.p < c.end) {
			uint32_t npairs = 0;
			bool isNil = false;
			if (!mpMapHeader(c, npairs, isNil)) break;
			if (isNil) break;                          // centinela: fin de registros

			const uint8_t* crcBin  = NULL; uint32_t crcLen  = 0;
			const uint8_t* nameStr = NULL; uint32_t nameLen = 0;
			bool parseErr = false;

			for (uint32_t i = 0; i < npairs; ++i) {
				const uint8_t* ks; uint32_t kl;
				if (!mpStrRef(c, ks, kl)) { parseErr = true; break; }   // las claves son strings
				if      (kl == 3 && std::memcmp(ks, "crc", 3) == 0)  { if (!mpBinRef(c, crcBin, crcLen))  { parseErr = true; break; } }
				else if (kl == 4 && std::memcmp(ks, "name", 4) == 0) { if (!mpStrRef(c, nameStr, nameLen)) { parseErr = true; break; } }
				else                                                 { if (!mpSkip(c, MP_MAX_DEPTH))       { parseErr = true; break; } }
			}
			if (parseErr) break;

			if (crcBin && crcLen == 4 && std::memcmp(crcBin, key, 4) == 0) {
				if (nameStr && nameLen > 0) {
					outName.assign((const char*)nameStr, (size_t)nameLen);
					ok = true;
				}
				break;                                 // era nuestro registro
			}
		}
	} while (0);

	std::free(buf);
	return ok;
}

std::string RdbReader::cleanName(const std::string& s) {
	return cleanNameImpl(s);
}

void RdbReader::findNamesByName(const std::string& rdbPath, const std::string& cleanQuery,
                                std::vector<std::string>& results, int maxResults) {
	results.clear();
	if (cleanQuery.empty() || maxResults <= 0)
		return;

	FILE* f = std::fopen(rdbPath.c_str(), "rb");
	if (!f) return;
	if (std::fseek(f, 0, SEEK_END) != 0) { std::fclose(f); return; }
	long sz = std::ftell(f);
	if (sz <= 16 || sz > RDB_MAX_SIZE) { std::fclose(f); return; }
	if (std::fseek(f, 0, SEEK_SET) != 0) { std::fclose(f); return; }

	uint8_t* buf = (uint8_t*)std::malloc((size_t)sz);
	if (!buf) { std::fclose(f); return; }
	bool readOk = std::fread(buf, 1, (size_t)sz, f) == (size_t)sz;
	std::fclose(f);
	if (!readOk) { std::free(buf); return; }

	if (std::memcmp(buf, "RARCHDB", 7) == 0) {
		uint64_t metaOff = 0;
		for (int i = 8; i < 16; ++i) metaOff = (metaOff << 8) | (uint64_t)buf[i];
		if (metaOff > (uint64_t)sz) metaOff = (uint64_t)sz;

		Cur c;
		c.p   = buf + 16;
		c.end = buf + metaOff;

		while (c.p < c.end && (int)results.size() < maxResults) {
			uint32_t npairs = 0;
			bool isNil = false;
			if (!mpMapHeader(c, npairs, isNil)) break;
			if (isNil) break;

			const uint8_t* nameStr = NULL; uint32_t nameLen = 0;
			bool parseErr = false;
			for (uint32_t i = 0; i < npairs; ++i) {
				const uint8_t* ks; uint32_t kl;
				if (!mpStrRef(c, ks, kl)) { parseErr = true; break; }
				if (kl == 4 && std::memcmp(ks, "name", 4) == 0) {
					if (!mpStrRef(c, nameStr, nameLen)) { parseErr = true; break; }
				} else {
					if (!mpSkip(c, MP_MAX_DEPTH)) { parseErr = true; break; }
				}
			}
			if (parseErr) break;

			if (nameStr && nameLen > 0) {
				std::string raw((const char*)nameStr, (size_t)nameLen);
				if (cleanNameImpl(raw) == cleanQuery) {
					bool dup = false;
					for (std::size_t k = 0; k < results.size(); ++k)
						if (results[k] == raw) { dup = true; break; }
					if (!dup)
						results.push_back(raw);
				}
			}
		}
	}

	std::free(buf);
}
