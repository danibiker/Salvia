#pragma once

#include <string>
#include <vector>

/* Descompresion de ficheros .7z (LZMA SDK 19.00 en C, copia en src/unzip/lzma,
 * solo la parte de descompresion).
 *
 * Soporta los metodos habituales de 7-Zip: LZMA, LZMA2, Copy y los filtros
 * BCJ/BCJ2/ARM/ARMT/PPC/SPARC/IA64/Delta. No soporta PPMd (poco habitual) ni
 * archivos cifrados con contrasena.
 *
 * Memoria: el SDK descomprime cada bloque solido ENTERO en RAM antes de
 * escribir sus ficheros. Con .7z solidos de cientos de MB puede no caber en la
 * Xbox 360; para paquetes pequenos o medianos no hay problema. */
namespace SevenZip {

	/* Extrae el contenido de archivePath en destDir, creando las subcarpetas.
	 *
	 * onlyThese: si no es NULL, solo se extraen esas rutas (relativas a la raiz
	 *            del .7z, con '/' o '\\', sin distinguir mayusculas). Las que no
	 *            esten en el .7z se ignoran.
	 * progress:  si no es NULL, recibe la fraccion 0..1 de bytes extraidos
	 *            (se actualiza al terminar cada fichero).
	 * error:     si no es NULL, recibe el motivo cuando devuelve false.
	 *
	 * Devuelve true si se extrajo todo lo pedido sin errores. */
	bool extract(const std::string& archivePath, const std::string& destDir,
	             const std::vector<std::string>* onlyThese = NULL,
	             float* progress = NULL, std::string* error = NULL);

	/* Lista las rutas de los ficheros (no carpetas) que contiene el .7z, con
	 * '/' como separador. Devuelve false si no se puede abrir. */
	bool list(const std::string& archivePath, std::vector<std::string>& files,
	          std::string* error = NULL);
}
