#include "sevenzip.h"

#include <io/dirutil.h>
#include <const/constant.h>
#include <utils/logger.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <set>

/* Los fuentes del SDK incluyen salvia_7z_rename.h desde 7zTypes.h, asi que
 * aqui tambien se llama a las versiones con prefijo s7z_. */
#include "lzma/7z.h"
#include "lzma/7zAlloc.h"
#include "lzma/7zCrc.h"
#include "lzma/7zFile.h"

namespace {

const size_t kInputBufSize = (size_t)1 << 18;   // buffer de lectura del .7z (256 KB)

const char* textoError(SRes res) {
	switch (res) {
		case SZ_ERROR_DATA:        return "datos corruptos";
		case SZ_ERROR_MEM:         return "sin memoria (bloque solido demasiado grande?)";
		case SZ_ERROR_CRC:         return "CRC incorrecto";
		case SZ_ERROR_UNSUPPORTED: return "metodo de compresion no soportado (PPMd, cifrado...)";
		case SZ_ERROR_INPUT_EOF:   return "el .7z esta truncado";
		case SZ_ERROR_READ:        return "error de lectura";
		case SZ_ERROR_WRITE:       return "error de escritura";
		case SZ_ERROR_ARCHIVE:     return "archivo .7z no valido";
		case SZ_ERROR_NO_ARCHIVE:  return "no es un archivo .7z";
		default:                   return "error desconocido";
	}
}

void ponError(std::string* error, const std::string& msg) {
	LOG_ERROR("SevenZip: %s\n", msg.c_str());
	if (error) *error = msg;
}

/* Nombre UTF-16 del .7z -> char. En Windows se pasa a la pagina de codigos
 * ANSI (lo que entienden fopen y CreateDirectoryA); en la Xbox 360 el sistema
 * de ficheros solo admite ASCII. Lo que no se puede representar queda como '_'. */
std::string utf16ALocal(const UInt16* s, size_t len) {
	std::string out;
	if (len == 0) return out;
#if defined(_WIN32) && !defined(_XBOX)
	int n = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)s, (int)len, NULL, 0, "_", NULL);
	if (n > 0) {
		out.resize(n);
		WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)s, (int)len, &out[0], n, "_", NULL);
	}
#else
	out.reserve(len);
	for (size_t i = 0; i < len; i++)
		out += (s[i] < 0x80) ? (char)s[i] : '_';
#endif
	return out;
}

/* Clave para comparar rutas: separador '/' y minusculas. */
std::string claveRuta(const std::string& ruta) {
	std::string k(ruta);
	for (size_t i = 0; i < k.size(); i++) {
		if (k[i] == '\\') k[i] = '/';
		k[i] = (char)tolower((unsigned char)k[i]);
	}
	while (!k.empty() && k[0] == '/') k.erase(0, 1);
	return k;
}

/* Ruta relativa del .7z -> ruta segura para el disco, con el separador de la
 * plataforma. Devuelve "" si intenta salir de destDir ("..") o queda vacia. */
std::string rutaSegura(const std::string& rel) {
	const std::string sep = Constant::getFileSep();
	std::string out, comp;
	for (size_t i = 0; i <= rel.size(); i++) {
		const char c = (i < rel.size()) ? rel[i] : '/';
		if (c == '/' || c == '\\') {
			if (comp == "..") return "";
			if (!comp.empty() && comp != ".") {
				if (!out.empty()) out += sep;
				out += comp;
			}
			comp.clear();
		} else if ((unsigned char)c < 0x20 || strchr("<>:\"|?*", c)) {
			comp += '_';
		} else {
			comp += c;
		}
	}
	return out;
}

/* Carpeta que contiene una ruta (hasta el ultimo separador). */
std::string carpetaDe(const std::string& ruta) {
	const size_t p = ruta.find_last_of("\\/");
	return (p == std::string::npos) ? std::string() : ruta.substr(0, p);
}

/* .7z abierto: stream de entrada, buffer de lectura y base de datos. */
struct Archivo7z {
	ISzAlloc allocImp;
	ISzAlloc allocTempImp;
	CFileInStream archiveStream;
	CLookToRead2 lookStream;
	CSzArEx db;
	bool ficheroAbierto;
	bool dbAbierta;

	Archivo7z() : ficheroAbierto(false), dbAbierta(false) {
		allocImp.Alloc = SzAlloc;
		allocImp.Free = SzFree;
		allocTempImp.Alloc = SzAllocTemp;
		allocTempImp.Free = SzFreeTemp;
		lookStream.buf = NULL;
	}

	~Archivo7z() {
		if (dbAbierta) SzArEx_Free(&db, &allocImp);
		if (lookStream.buf) ISzAlloc_Free(&allocImp, lookStream.buf);
		if (ficheroAbierto) File_Close(&archiveStream.file);
	}

	bool abrir(const std::string& ruta, std::string* error) {
		static bool tablaCrc = false;
		if (!tablaCrc) { CrcGenerateTable(); tablaCrc = true; }

		if (InFile_Open(&archiveStream.file, ruta.c_str()) != 0) {
			ponError(error, "no se puede abrir " + ruta);
			return false;
		}
		ficheroAbierto = true;

		FileInStream_CreateVTable(&archiveStream);
		LookToRead2_CreateVTable(&lookStream, False);
		lookStream.buf = (Byte*)ISzAlloc_Alloc(&allocImp, kInputBufSize);
		if (!lookStream.buf) {
			ponError(error, "sin memoria para el buffer de lectura");
			return false;
		}
		lookStream.bufSize = kInputBufSize;
		lookStream.realStream = &archiveStream.vt;
		LookToRead2_Init(&lookStream);

		SzArEx_Init(&db);
		dbAbierta = true;
		const SRes res = SzArEx_Open(&db, &lookStream.vt, &allocImp, &allocTempImp);
		if (res != SZ_OK) {
			ponError(error, ruta + ": " + textoError(res));
			return false;
		}
		return true;
	}

	/* Ruta del fichero i dentro del .7z ('/' como separador). */
	std::string nombre(UInt32 i, std::vector<UInt16>& buf) {
		const size_t len = SzArEx_GetFileNameUtf16(&db, i, NULL);   // incluye el 0 final
		if (len == 0) return std::string();
		if (buf.size() < len) buf.resize(len);
		SzArEx_GetFileNameUtf16(&db, i, &buf[0]);
		return utf16ALocal(&buf[0], len - 1);
	}
};

} // namespace

namespace SevenZip {

bool list(const std::string& archivePath, std::vector<std::string>& files, std::string* error) {
	files.clear();
	Archivo7z a;
	if (!a.abrir(archivePath, error)) return false;

	std::vector<UInt16> buf;
	for (UInt32 i = 0; i < a.db.NumFiles; i++) {
		if (SzArEx_IsDir(&a.db, i)) continue;
		std::string n = a.nombre(i, buf);
		for (size_t k = 0; k < n.size(); k++) if (n[k] == '\\') n[k] = '/';
		files.push_back(n);
	}
	return true;
}

bool extract(const std::string& archivePath, const std::string& destDir,
             const std::vector<std::string>* onlyThese, float* progress, std::string* error) {
	if (progress) *progress = 0.0f;

	Archivo7z a;
	if (!a.abrir(archivePath, error)) return false;

	std::set<std::string> pedidos;
	if (onlyThese) {
		for (size_t k = 0; k < onlyThese->size(); k++)
			pedidos.insert(claveRuta((*onlyThese)[k]));
	}

	/* Total de bytes a extraer, para el progreso. */
	std::vector<UInt16> nombreBuf;
	UInt64 total = 0;
	for (UInt32 i = 0; i < a.db.NumFiles; i++) {
		if (SzArEx_IsDir(&a.db, i)) continue;
		if (onlyThese && !pedidos.count(claveRuta(a.nombre(i, nombreBuf)))) continue;
		total += SzArEx_GetFileSize(&a.db, i);
	}

	const std::string sep = Constant::getFileSep();
	dirutil dir;
	dir.createDirRecursive(destDir.c_str());

	/* Cache del bloque descomprimido: SzArEx_Extract lo reutiliza entre
	 * ficheros del mismo bloque solido. */
	UInt32 blockIndex = 0xFFFFFFFF;
	Byte* outBuffer = NULL;
	size_t outBufferSize = 0;

	UInt64 hecho = 0;
	bool ok = true;
	size_t extraidos = 0;

	for (UInt32 i = 0; i < a.db.NumFiles && ok; i++) {
		const std::string rel = a.nombre(i, nombreBuf);
		const bool esDir = SzArEx_IsDir(&a.db, i) != 0;

		if (onlyThese) {
			if (esDir) continue;   // las carpetas necesarias se crean con cada fichero
			if (!pedidos.count(claveRuta(rel))) continue;
			pedidos.erase(claveRuta(rel));
		}

		const std::string relSegura = rutaSegura(rel);
		if (relSegura.empty()) {
			LOG_DEBUG("SevenZip: se ignora la entrada '%s' (ruta no valida)\n", rel.c_str());
			continue;
		}
		const std::string destino = destDir + sep + relSegura;

		if (esDir) {
			dir.createDirRecursive(destino.c_str());
			continue;
		}

		const std::string carpeta = carpetaDe(destino);
		if (!carpeta.empty()) dir.createDirRecursive(carpeta.c_str());

		size_t offset = 0;
		size_t outSizeProcessed = 0;
		const SRes res = SzArEx_Extract(&a.db, &a.lookStream.vt, i, &blockIndex,
			&outBuffer, &outBufferSize, &offset, &outSizeProcessed,
			&a.allocImp, &a.allocTempImp);
		if (res != SZ_OK) {
			ponError(error, rel + ": " + textoError(res));
			ok = false;
			break;
		}

		FILE* f = fopen(destino.c_str(), "wb");
		if (!f) {
			ponError(error, "no se puede crear " + destino);
			ok = false;
			break;
		}
		const size_t escrito = outSizeProcessed ? fwrite(outBuffer + offset, 1, outSizeProcessed, f) : 0;
		const bool cerradoOk = (fclose(f) == 0);
		if (escrito != outSizeProcessed || !cerradoOk) {
			remove(destino.c_str());
			ponError(error, "error escribiendo " + destino);
			ok = false;
			break;
		}

		extraidos++;
		hecho += outSizeProcessed;
		if (progress) *progress = total ? (float)((double)hecho / (double)total) : 1.0f;
	}

	if (outBuffer) ISzAlloc_Free(&a.allocImp, outBuffer);

	for (std::set<std::string>::const_iterator it = pedidos.begin(); ok && it != pedidos.end(); ++it)
		LOG_DEBUG("SevenZip: '%s' no esta en %s\n", it->c_str(), archivePath.c_str());

	LOG_DEBUG("SevenZip: %s -> %s: %u ficheros extraidos%s\n", archivePath.c_str(), destDir.c_str(),
	          (unsigned)extraidos, ok ? "" : " (con error)");
	if (ok && progress) *progress = 1.0f;
	return ok;
}

} // namespace SevenZip
