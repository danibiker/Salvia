#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <cstdarg> // Necesario para va_list
#include <vector>

#define NUM_LOGS_TO_FLUSH 20

enum {
    L_DEBUG = 0,
    L_INFO,
    L_ERROR,
    L_MAX // Auxiliar para control de rangos
};

class Logger {
private:
    std::ofstream logFile;
    unsigned int numLogs;
    int errorLevel;
    const char* ERRLEVELSTXT[L_MAX];

public:
    Logger(const char* filename);

    ~Logger() {
        if (logFile.is_open()) {
            logFile.flush();
            logFile.close();
        }
    }

    // Función principal con soporte de formato printf
    void write(int level, const char* fmt, ...);

    // Helpers con formato
    void debug(const char* fmt, ...);
    void info(const char* fmt, ...);
    void error(const char* fmt, ...);
};

