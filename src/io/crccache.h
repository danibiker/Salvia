#pragma once

#include <map>
#include <string>
#include <stdint.h>

/* Cache de CRC32 de ficheros en disco.
 *
 * Calcular el CRC obliga a leer el fichero entero, y lo caro es la lectura
 * (disco, USB o red), no el calculo. La cache guarda, por fichero, el tamano,
 * la fecha de ultima modificacion y el CRC que salio; mientras tamano y fecha
 * no cambien se devuelve el CRC guardado sin abrir el fichero (solo se pide
 * GetFileAttributesEx).
 *
 * Se guarda en un fichero de texto aparte, una linea por fichero:
 *     tamano|fecha|crc|ruta
 * (la ruta va al final para que pueda contener cualquier caracter). No toca
 * system_bios.xml: ese XML es el dato de referencia (el CRC esperado) y la
 * cache es lo observado en el disco de cada usuario.
 *
 * No es segura entre hilos: se usa desde un solo hilo. */
class CrcCache {
public:
	explicit CrcCache(const std::string& cacheFile);
	~CrcCache();   // guarda si hubo cambios

	/* CRC32 del fichero completo. Devuelve false si el fichero no existe o
	 * no se puede leer (crc queda a 0). */
	bool getCrc(const std::string& path, uint32_t& crc);

	/* Escribe la cache a disco si hubo cambios. Devuelve false si no se pudo. */
	bool save();

private:
	struct Entry {
		uint64_t size;
		uint64_t mtime;   // FILETIME de ultima escritura, como entero de 64 bits
		uint32_t crc;
		std::string path; // ruta tal cual se pidio (la clave va en minusculas)
	};

	std::map<std::string, Entry> entries;   // clave: ruta en minusculas
	std::string cacheFile;
	bool dirty;

	void load();
	static std::string makeKey(const std::string& path);
	static bool fileStamp(const std::string& path, uint64_t& size, uint64_t& mtime);
};
