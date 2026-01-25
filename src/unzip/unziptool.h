#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unzip/minizip-1.2.5/unzip.h>
#include <unzip/zlib.h>

// Estructura de salida
struct unzippedFileInfo {
    int errorCode;
    std::string originalPath;      // El .zip original
    std::string extractedPath;     // Ruta en disco si se extrajo
    std::string fileNameInsideZip; // Nombre del fichero real (ej: "Sonic.bin")
    std::size_t romsize;
    void* memoryBuffer;

    unzippedFileInfo() : errorCode(-1), romsize(0), memoryBuffer(nullptr) {}
};

// Función auxiliar para separar las extensiones y normalizarlas
std::vector<std::string> splitExtensions(const std::string& extensions) {
    std::vector<std::string> list;
    std::stringstream ss(extensions);
    std::string ext;
    while (ss >> ext) {
        // Aseguramos que empiecen por punto para la comparacion
        if (ext[0] != '.') ext = "." + ext;
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        list.push_back(ext);
    }
    return list;
}

unzippedFileInfo unzipOrLoad(const std::string& rompath, const std::string& extensions, bool toMemory, const std::string& tempDir) {
    unzippedFileInfo ret;
    ret.originalPath = rompath;
    std::vector<std::string> allowedExts = splitExtensions(extensions);
	// Límite de 50 MB (50 * 1024 * 1024 bytes)
    const std::size_t MAX_MEM_SIZE = 52428800;

    // 1. Detección de ZIP por Magic Number
    FILE* fTest = fopen(rompath.c_str(), "rb");
    if (!fTest) return ret;
    uint32_t magic = 0;
    fread(&magic, 4, 1, fTest);
    fclose(fTest);

	#ifdef _XBOX
    // Función nativa del SDK de Xbox para invertir bytes de 32 bits
    magic = _byteswap_ulong(magic); 
	#endif

    // Si NO es un ZIP (PK\003\004)
    // --- CASO: NO ES UN ZIP ---
    if (magic != 0x04034b50) {
        FILE* f = fopen(rompath.c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            ret.romsize = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (toMemory) {
                // Validación de tamaño para archivos normales
                if (ret.romsize > MAX_MEM_SIZE) {
                    fclose(f);
                    ret.errorCode = -2; // Código para "Archivo demasiado grande para RAM"
                    return ret;
                }
                ret.memoryBuffer = malloc(ret.romsize);
                if (ret.memoryBuffer) fread(ret.memoryBuffer, 1, ret.romsize, f);
            }
            ret.extractedPath = rompath;
            ret.errorCode = 0;
            fclose(f);
        }
        return ret;
    }

    // 2. Es un ZIP, abrir con Minizip
    unzFile uf = unzOpen(rompath.c_str());
    if (!uf) return ret;

    bool found = false;
    int res = unzGoToFirstFile(uf);
    while (res == UNZ_OK) {
        char filenameInZip[256];
        unz_file_info fileInfo;
        unzGetCurrentFileInfo(uf, &fileInfo, filenameInZip, sizeof(filenameInZip), NULL, 0, NULL, 0);

        std::string currentFile = filenameInZip;
        std::string currentExt = "";
        std::size_t dotPos = currentFile.find_last_of(".");
        if (dotPos != std::string::npos) {
            currentExt = currentFile.substr(dotPos);
            std::transform(currentExt.begin(), currentExt.end(), currentExt.begin(), ::tolower);
        }

        // Comprobar si la extensión del fichero actual está en nuestra lista
        bool match = false;
		for (std::size_t i = 0; i < allowedExts.size(); ++i) {
			if (currentExt == allowedExts[i]) {
				match = true;
				break;
			}
		}

        if (match) {
            if (unzOpenCurrentFile(uf) == UNZ_OK) {
                ret.romsize = fileInfo.uncompressed_size;
                ret.fileNameInsideZip = currentFile;

                if (toMemory) {
					// VALIDACIÓN CRÍTICA: No cargar si supera 50MB
					if (ret.romsize > MAX_MEM_SIZE) {
						unzClose(uf);
						ret.errorCode = -2; 
						return ret;
					}

					if (unzOpenCurrentFile(uf) == UNZ_OK) {
						ret.memoryBuffer = malloc(ret.romsize);
						if (ret.memoryBuffer) {
							unzReadCurrentFile(uf, ret.memoryBuffer, (unsigned int)ret.romsize);
							found = true;
						}
						unzCloseCurrentFile(uf);
					}
				} else {
					ret.extractedPath = tempDir + Constant::getFileSep() + "extractedRom" + currentExt;
                    FILE* fout = fopen(ret.extractedPath.c_str(), "wb");
                    if (fout) {
                        std::vector<char> buffer(16384);
                        int read;
                        while ((read = unzReadCurrentFile(uf, buffer.data(), (unsigned int)buffer.size())) > 0) {
                            fwrite(buffer.data(), 1, read, fout);
                        }
                        fclose(fout);
                        found = true;
                    }
                }
                unzCloseCurrentFile(uf);
                if (found) break;
            }
        }
        res = unzGoToNextFile(uf);
    }

    unzClose(uf);
    if (found) ret.errorCode = 0;
    return ret;
}