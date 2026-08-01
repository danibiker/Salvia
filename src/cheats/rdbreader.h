#ifndef CHEATS_RDBREADER_H
#define CHEATS_RDBREADER_H

#include <string>
#include <vector>
#include <stdint.h>

// Lector minimo y autonomo de bases de datos RetroArch (.rdb), pensado para un
// unico caso: dado el CRC32 de una ROM, devolver el nombre canonico (No-Intro)
// del juego, que es el nombre del fichero .cht correspondiente.
//
// NO depende de libretro-common (nada de intfstream / file_stream / strl /
// retro_endianness): usa stdio y lee los enteros byte a byte, asi que es correcto
// en cualquier endianness (incluida la Xbox 360, big-endian).
//
// Formato (libs/libretro-db es solo referencia): cabecera "RARCHDB\0" (8) +
// metadata_offset (u64 big-endian, 8), y del byte 16 hasta metadata_offset una
// secuencia de registros (mapas msgpack), cerrada por un centinela nil. Los .rdb
// oficiales de libretro-database NO llevan indices (los construye RetroArch en
// runtime), asi que se hace escaneo lineal: se carga el .rdb en memoria y se recorren
// los registros comparando el campo binario "crc" (4 bytes big-endian). Al acertar se
// devuelve el campo string "name". Coste O(tamaño), una vez por carga de juego.
class RdbReader {
public:
	// Busca el registro cuyo campo "crc" == crc y devuelve su campo "name".
	// Devuelve false si: no existe el fichero, no hay indice "crc", el CRC no
	// esta, o el registro no tiene "name". Solo lectura.
	static bool findNameByCrc(const std::string& rdbPath, uint32_t crc, std::string& outName);

	// Escaneo por NOMBRE (para juegos sin CRC utilizable, p.ej. imagenes de CD):
	// devuelve los nombres canonicos ("name") cuyo nombre normalizado coincide con
	// cleanQuery. Normaliza la consulta con cleanName() antes de llamar. maxResults
	// limita el numero de coincidencias.
	static void findNamesByName(const std::string& rdbPath, const std::string& cleanQuery,
	                            std::vector<std::string>& results, int maxResults);

	// Normaliza un nombre para comparar: minusculas, sin el contenido de ()/[], solo
	// alfanumerico con espacios colapsados. (Consistente con la comparacion interna.)
	static std::string cleanName(const std::string& s);
};

#endif // CHEATS_RDBREADER_H
