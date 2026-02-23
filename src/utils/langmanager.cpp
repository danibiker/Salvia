#include "langmanager.h"

#include <fstream>
#include <sstream>

LanguageManager* LanguageManager::m_instance = NULL;

LanguageManager* LanguageManager::instance() {
    if (m_instance == NULL) m_instance = new LanguageManager();
    return m_instance;
}

bool LanguageManager::loadLanguage(const std::string& filename) {
    m_texts.clear();

    // En Xbox 360 las rutas suelen ser "game:\media\lang\es.ini" 
    // o relativas si el motor lo soporta.
    std::ifstream file(filename.c_str());
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        // Ignorar líneas vacías o comentarios
        if (line.empty() || line[0] == ';' || line[0] == '[') continue;

        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);

            // Limpiar posibles espacios en blanco al final (trim)
            size_t last = value.find_last_not_of("\r\n");
            if (last != std::string::npos) value = value.substr(0, last + 1);

            m_texts[key] = value;
        }
    }

    file.close();
    return !m_texts.empty();
}

std::string LanguageManager::get(const std::string& key) {
    std::map<std::string, std::string>::iterator it = m_texts.find(key);
    if (it != m_texts.end()) {
        return it->second;
    }
    return "[" + key + "]"; 
}