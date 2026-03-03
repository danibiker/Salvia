#include "CHDHashed.h"
#include <rc_hash.h>

// Forzamos la inclusión de los métodos que ya tienes definidos
// Asegúrate de que el archivo donde pusiste rc_hash_init_chd_cdreader se compile en el proyecto
extern void rc_hash_init_chd_cdreader(); 

CHDHashed::CHDHashed() : m_initialized(false) {
}

CHDHashed::~CHDHashed() {
}

void CHDHashed::InitSystems() {
	#ifdef HAVE_CHD
    if (!m_initialized) {
        // Inicializamos el lector de CHD que definimos antes
        rc_hash_init_chd_cdreader();
        m_initialized = true;
    }
	#endif
}

std::string CHDHashed::GetHash(const std::string& filePath, int consoleId) {
    #ifdef HAVE_CHD
	InitSystems();

    char hashBuffer[33]; // MD5 son 32 chars + null
    memset(hashBuffer, 0, sizeof(hashBuffer));

    // Intentamos generar el hash
    // En Xbox 360, asegúrate de que la ruta use el formato de dispositivo (e.g. "game:\file.chd")
    if (rc_hash_generate_from_file(hashBuffer, consoleId, filePath.c_str())) {
        return std::string(hashBuffer);
    }
	#endif
    return "";
}