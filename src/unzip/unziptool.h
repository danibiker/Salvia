#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unzip/minizip-1.2.5/unzip.h>
#include <unzip/zlib.h>
#include "unziptool_common.h"

#if defined(_XBOX) || defined(_XBOX360)
    #include <xtl.h>
    #include <io.h> // Para _commit y _fileno
#endif

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
    ret.errorCode = -1; // Por defecto error
    ret.memoryBuffer = NULL;
    ret.romsize = 0;

    std::vector<std::string> allowedExts = splitExtensions(extensions);
    const std::size_t MAX_MEM_SIZE = 52428800; // 50 MB

    // 1. Detección de ZIP por Magic Number (Endian-Safe)
    FILE* fTest = fopen(rompath.c_str(), "rb");
    if (!fTest) {
        LOG_DEBUG("Unable to open file: %s\n", rompath.c_str());
        return ret;
    }
    uint32_t magic = 0;
    fread(&magic, 4, 1, fTest);
    fclose(fTest);

    // En Xbox (Big Endian), el magic de PKZip (0x50 0x4B 0x03 0x04) 
    // se lee de forma distinta. Normalizamos para la comparación.
    #ifdef _XBOX
        magic = _byteswap_ulong(magic); 
    #endif

    // --- CASO 1: NO ES UN ZIP (Carga directa) ---
    if (magic != 0x04034b50) {
        FILE* f = fopen(rompath.c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            ret.romsize = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (toMemory) {
                if (ret.romsize > MAX_MEM_SIZE) {
                    fclose(f);
                    ret.errorCode = -2; 
                    LOG_DEBUG("File too big for RAM: %Iu\n", ret.romsize);
                    return ret;
                }
                ret.memoryBuffer = malloc(ret.romsize);
                if (ret.memoryBuffer) fread(ret.memoryBuffer, 1, ret.romsize, f);
            }
            ret.extractedPath = rompath;
            ret.errorCode = 0;
            fclose(f);
            LOG_DEBUG("Normal file loaded. Size: %Iu\n", ret.romsize);
        }
        return ret;
    }

    // --- CASO 2: ES UN ZIP (Extracción con Minizip) ---
    LOG_DEBUG("Zip detected. Searching for compatible extension...\n");
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

        // Comprobación de extensión
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
                    if (ret.romsize > MAX_MEM_SIZE) {
                        ret.errorCode = -2;
                    } else {
                        ret.memoryBuffer = malloc(ret.romsize);
                        if (ret.memoryBuffer) {
                            unzReadCurrentFile(uf, ret.memoryBuffer, (unsigned int)ret.romsize);
                            found = true;
                        }
                    }
                } else {
                    // Extracción a disco (Temp)
                    ret.extractedPath = tempDir + Constant::getFileSep() + "extractedRom" + currentExt;
                    FILE* fout = fopen(ret.extractedPath.c_str(), "wb");
                    if (fout) {
                        // Buffer de 64KB: Óptimo para el DMA del HDD de Xbox 360
                        std::vector<char> writeBuf(65536); 
                        int bytesRead;
                        while ((bytesRead = unzReadCurrentFile(uf, writeBuf.data(), (unsigned int)writeBuf.size())) > 0) {
                            fwrite(writeBuf.data(), 1, bytesRead, fout);
                        }
                        
                        // SYNC CRÍTICO: Asegurar que el Core vea el archivo completo
                        fflush(fout);
                        #ifdef _XBOX
                            _commit(_fileno(fout)); 
                        #endif
                        fclose(fout);
                        found = true;
                    } else {
						LOG_DEBUG("Error extracting to file\n");
					}
                }
                unzCloseCurrentFile(uf); 
                if (found || ret.errorCode == -2) break;
            }
        }
        res = unzGoToNextFile(uf);
    }

    unzClose(uf);
    if (found) {
        ret.errorCode = 0;
        LOG_DEBUG("Extraction successful: %s\n", toMemory ? "RAM" : "DISK");
    }
    return ret;
}