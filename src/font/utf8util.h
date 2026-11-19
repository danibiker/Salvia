#ifndef UTF8UTIL_H
#define UTF8UTIL_H

#include <string>
#include <cstddef>

/* Utilidades minimas para recorrer texto UTF-8 por caracteres (no por bytes)
 * y decidir donde se puede partir una linea en textos chinos, japoneses o
 * coreanos, que no separan las palabras con espacios. */
namespace utf8 {

	/* Byte de continuacion (10xxxxxx): no es inicio de caracter. */
	inline bool isCont(unsigned char c){
		return (c & 0xC0) == 0x80;
	}

	/* Indice del siguiente caracter a partir de pos. */
	inline std::size_t next(const std::string& s, std::size_t pos){
		const std::size_t n = s.size();
		if (pos >= n) return n;
		++pos;
		while (pos < n && isCont((unsigned char)s[pos])) ++pos;
		return pos;
	}

	/* Indice del caracter anterior a pos. */
	inline std::size_t prev(const std::string& s, std::size_t pos){
		if (pos == 0) return 0;
		if (pos > s.size()) pos = s.size();
		--pos;
		while (pos > 0 && isCont((unsigned char)s[pos])) --pos;
		return pos;
	}

	/* Ajusta pos hacia atras hasta el inicio de un caracter. */
	inline std::size_t floorBoundary(const std::string& s, std::size_t pos){
		if (pos >= s.size()) return s.size();
		while (pos > 0 && isCont((unsigned char)s[pos])) --pos;
		return pos;
	}

	/* Ajusta pos hacia delante hasta el inicio de un caracter. */
	inline std::size_t ceilBoundary(const std::string& s, std::size_t pos){
		const std::size_t n = s.size();
		while (pos < n && isCont((unsigned char)s[pos])) ++pos;
		return pos < n ? pos : n;
	}

	/* Decodifica el caracter que empieza en s[pos] y deja en len los bytes
	 * que ocupa. Las secuencias mal formadas devuelven 0xFFFD y avanzan 1. */
	inline unsigned int decode(const std::string& s, std::size_t pos, std::size_t& len){
		const std::size_t n = s.size();
		len = 1;
		if (pos >= n) return 0;
		const unsigned char c = (unsigned char)s[pos];
		if (c < 0x80) return c;

		unsigned int cp;
		std::size_t need;
		if ((c & 0xE0) == 0xC0)      { cp = c & 0x1F; need = 1; }
		else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; need = 2; }
		else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; need = 3; }
		else return 0xFFFD;

		if (pos + need >= n) return 0xFFFD;   // secuencia cortada al final
		for (std::size_t i = 1; i <= need; i++){
			const unsigned char cc = (unsigned char)s[pos + i];
			if (!isCont(cc)) return 0xFFFD;
			cp = (cp << 6) | (cc & 0x3F);
		}
		len = need + 1;
		return cp;
	}

	/* Caracteres de escritura china o japonesa: se puede partir la linea
	 * antes o despues de cada uno. El coreano (hangul) usa espacios entre
	 * palabras, asi que no se incluye. */
	inline bool isCjkBreakable(unsigned int cp){
		return (cp >= 0x2E80 && cp <= 0x2FDF)    // radicales CJK y Kangxi
			|| (cp >= 0x3000 && cp <= 0x30FF)    // puntuacion CJK, hiragana, katakana
			|| (cp >= 0x3100 && cp <= 0x312F)    // bopomofo
			|| (cp >= 0x31A0 && cp <= 0x31FF)    // bopomofo ext., trazos, katakana ext.
			|| (cp >= 0x3400 && cp <= 0x4DBF)    // ideogramas ext. A
			|| (cp >= 0x4E00 && cp <= 0x9FFF)    // ideogramas unificados
			|| (cp >= 0xF900 && cp <= 0xFAFF)    // ideogramas de compatibilidad
			|| (cp >= 0xFE30 && cp <= 0xFE4F)    // formas de compatibilidad CJK
			|| (cp >= 0xFF00 && cp <= 0xFFEF)    // formas de ancho completo y medio
			|| (cp >= 0x20000 && cp <= 0x3FFFF); // ideogramas ext. B en adelante
	}

	/* Caracteres que no pueden empezar una linea (cierres, puntuacion,
	 * kana pequenyos y marca de vocal larga). */
	inline bool noBreakBefore(unsigned int cp){
		switch (cp){
			case '!': case ')': case ',': case '.': case ':': case ';': case '?': case ']': case '}':
			case 0x2019: case 0x201D: case 0x2026: case 0x2025:            // comillas de cierre, puntos suspensivos
			case 0x3001: case 0x3002: case 0x3005:                         // coma, punto, marca de repeticion
			case 0x3009: case 0x300B: case 0x300D: case 0x300F: case 0x3011:
			case 0x3015: case 0x3017: case 0x3019: case 0x301B: case 0x301E: case 0x301F:
			case 0x3041: case 0x3043: case 0x3045: case 0x3047: case 0x3049: // hiragana pequenyos
			case 0x3063: case 0x3083: case 0x3085: case 0x3087: case 0x308E: case 0x3095: case 0x3096:
			case 0x309B: case 0x309C: case 0x309D: case 0x309E:
			case 0x30A1: case 0x30A3: case 0x30A5: case 0x30A7: case 0x30A9: // katakana pequenyos
			case 0x30C3: case 0x30E3: case 0x30E5: case 0x30E7: case 0x30EE: case 0x30F5: case 0x30F6:
			case 0x30FB: case 0x30FC: case 0x30FD: case 0x30FE:              // punto medio, vocal larga
			case 0xFF01: case 0xFF09: case 0xFF0C: case 0xFF0E: case 0xFF1A: case 0xFF1B:
			case 0xFF1F: case 0xFF3D: case 0xFF5D: case 0xFF60: case 0xFF61: case 0xFF63: case 0xFF64:
				return true;
			default:
				return (cp >= 0x31F0 && cp <= 0x31FF); // katakana pequenyos extendidos
		}
	}

	/* Caracteres que no pueden terminar una linea (aperturas). */
	inline bool noBreakAfter(unsigned int cp){
		switch (cp){
			case '(': case '[': case '{':
			case 0x2018: case 0x201C:                                       // comillas de apertura
			case 0x3008: case 0x300A: case 0x300C: case 0x300E: case 0x3010:
			case 0x3014: case 0x3016: case 0x3018: case 0x301A: case 0x301D:
			case 0xFF08: case 0xFF3B: case 0xFF5B: case 0xFF5F: case 0xFF62:
				return true;
			default:
				return false;
		}
	}

	/* Fin del siguiente trozo que no se puede partir, empezando en pos.
	 * Entre dos caracteres se puede partir si alguno de ellos es chino o
	 * japones, salvo que lo impidan las reglas de puntuacion de arriba.
	 * En un texto sin caracteres CJK devuelve s.size() (la palabra entera). */
	inline std::size_t nextBreak(const std::string& s, std::size_t pos){
		const std::size_t n = s.size();
		if (pos >= n) return n;

		std::size_t len;
		unsigned int prevCp = decode(s, pos, len);
		pos += len;
		while (pos < n){
			const unsigned int cp = decode(s, pos, len);
			if ((isCjkBreakable(prevCp) || isCjkBreakable(cp))
				&& !noBreakBefore(cp) && !noBreakAfter(prevCp)) {
				return pos;
			}
			prevCp = cp;
			pos += len;
		}
		return n;
	}
}

#endif
