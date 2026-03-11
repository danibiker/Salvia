#pragma once

#include "logger.h"

#ifdef _XBOX
    #include <xtl.h>
#elif defined(WIN)
    #include <windows.h>
#endif

std::ofstream Logger::logFile;
unsigned int Logger::numLogs = 0;
char Logger::messageBuffer[2048];
const char* Logger::ERRLEVELSTXT[L_MAX] = { "DEBUG", "INFO", "ERROR" };
int Logger::errorLevel = L_DEBUG;

// CRÍTICO PARA XBOX 360: Protección multihilo
#ifdef _XBOX
    CRITICAL_SECTION logSync;
    static bool csInitialized = false;
#endif


Logger::Logger(const char* filename) {
    if (!logFile.is_open()) {
        logFile.open(filename, std::ios::app);
    }
    numLogs = 0;
#ifdef _XBOX
    if (!csInitialized) {
        InitializeCriticalSection(&logSync);
        csInitialized = true;
    }
#endif
}

void Logger::write(int level, const char* fmt, ...) {
    if (level < errorLevel || level >= L_MAX) return;

#ifdef _XBOX
    EnterCriticalSection(&logSync); // Evita que dos hilos rompan el messageBuffer
#endif

    // 1. Formateo
    va_list args;
    va_start(args, fmt);
    vsnprintf(messageBuffer, sizeof(messageBuffer), fmt, args);
    va_end(args);

    // 2. Tiempo
    time_t now = time(0);
    struct tm *timeinfo = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    // 3. Archivo
    /*if (logFile.is_open()){
        logFile << "[" << timestamp << "] [" << ERRLEVELSTXT[level] << "] " << messageBuffer << "\n";
        if (errorLevel == L_DEBUG || ++numLogs % NUM_LOGS_TO_FLUSH == 0) {
            logFile.flush();
        }
    }*/

    // 4. Debug Output (Imprescindible para desarrollo en Xbox 360)
    char debugOut[2100];
    _snprintf(debugOut, sizeof(debugOut), "[%s] [%s] %s\n", timestamp, ERRLEVELSTXT[level], messageBuffer);
    OutputDebugStringA(debugOut);

#ifdef _XBOX
    LeaveCriticalSection(&logSync);
#endif
}