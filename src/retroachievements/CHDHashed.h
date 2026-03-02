#ifndef CHD_HASHED_H
#define CHD_HASHED_H

#include <string>
#include <vector>

// Definición para el XDK si no están los tipos estándar
#ifndef UINT32
typedef unsigned __int32 uint32_t;
typedef unsigned __int8  uint8_t;
#endif

class CHDHashed {
public:
    CHDHashed();
    ~CHDHashed();

    // Devuelve el hash MD5 o una cadena vacía si falla
    std::string GetHash(const std::string& filePath, int consoleId);

private:
    bool m_initialized;
    void InitSystems();
};

#endif