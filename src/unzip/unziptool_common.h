#pragma once
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