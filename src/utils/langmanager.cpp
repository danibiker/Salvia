#include "langmanager.h"

LanguageManager* LanguageManager::m_instance = NULL;

LanguageManager* LanguageManager::instance() {
    if (m_instance == NULL) m_instance = new LanguageManager();
    return m_instance;
}

bool LanguageManager::loadLanguage(const std::string& filename) {
    m_texts.clear();

    // Convertimos nombre de archivo a W para la API de Windows
    std::wstring wFilename(filename.begin(), filename.end());
    if (filename.find(':') == std::string::npos && filename.substr(0, 2) != ".\\") {
        wFilename = L".\\" + wFilename;
    }

    wchar_t keyBuffer[4096];
    if (GetPrivateProfileStringW(L"Labels", NULL, L"", keyBuffer, 4096, wFilename.c_str()) > 0) {
        
        wchar_t* pKey = keyBuffer;
        while (*pKey) {
            std::wstring wKey = pKey;
            wchar_t valBuffer[512];
            
            GetPrivateProfileStringW(L"Labels", wKey.c_str(), L"", valBuffer, 512, wFilename.c_str());
            
            // --- CONVERSIÓN DE VALOR (WideChar a MultiByte) ---
            // Esto convierte el texto del .ini a un string normal respetando tildes locales
            int size_needed = WideCharToMultiByte(CP_ACP, 0, valBuffer, -1, NULL, 0, NULL, NULL);
            std::string strValue(size_needed - 1, 0);
            WideCharToMultiByte(CP_ACP, 0, valBuffer, -1, &strValue[0], size_needed, NULL, NULL);

            // Conversión de clave (simple)
            std::string sKey(wKey.begin(), wKey.end());
            
            m_texts[sKey] = strValue;
            pKey += wKey.length() + 1;
        }
        return true;
    }
    return false;
}

std::string LanguageManager::get(const std::string& key) {
    std::map<std::string, std::string>::iterator it = m_texts.find(key);
    if (it != m_texts.end()) {
        return it->second;
    }
    return "[" + key + "]"; 
}