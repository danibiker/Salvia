#pragma once

#include <cstdio>
#include <cstddef>
#ifdef _XBOX
#include <io.h>
#endif

// SRAM is stored as raw bytes, as in the existing save worker.
inline bool writeRawSave(const char* path, const void* buffer, std::size_t size) {
    if (!path || !buffer || size == 0) return false;
    FILE* file = std::fopen(path, "wb");
    if (!file) return false;
    bool ok = std::fwrite(buffer, 1, size, file) == size;
    if (std::fflush(file) != 0) ok = false;
#ifdef _XBOX
    if (_commit(_fileno(file)) != 0) ok = false;
#endif
    if (std::fclose(file) != 0) ok = false;
    return ok;
}
