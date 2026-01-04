#pragma once

#include "logger.h"

#ifdef _XBOX
    #include <xtl.h>
#elif defined(WIN)
    #include <windows.h>
#endif

std::ofstream Logger::logFile;
unsigned int Logger::numLogs;
const char* Logger::ERRLEVELSTXT[L_MAX];
int Logger::errorLevel;

Logger::Logger(const char* filename) {
    logFile.open(filename, std::ios::app);
    numLogs = 0;
        
    // Inicialización de textos
    ERRLEVELSTXT[L_DEBUG] = "DEBUG";
    ERRLEVELSTXT[L_INFO]  = "INFO";
    ERRLEVELSTXT[L_ERROR] = "ERROR";

#ifdef DEBUG
    errorLevel = L_DEBUG;
#else
    errorLevel = L_INFO; // En release solemos querer INFO y ERROR
#endif
}

void Logger::write(int level, const char* fmt, ...) {
    if (level < errorLevel || level >= L_MAX) return;

    // 1. Gestionar el formato del mensaje (printf style)
    char messageBuffer[1024]; // Buffer suficiente para un log
    va_list args;
    va_start(args, fmt);
    vsnprintf(messageBuffer, sizeof(messageBuffer), fmt, args);
    va_end(args);

    // 2. Obtener tiempo actual
    time_t now = time(0);
    struct tm *timeinfo = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    // 3. Escribir al archivo
    //logFile << "[" << timestamp << "] [" << ERRLEVELSTXT[level] << "] " << messageBuffer << std::endl;

	if (logFile.is_open()){
		logFile << "[" << timestamp << "] [" << ERRLEVELSTXT[level] << "] " << messageBuffer;
		// 4. Flush controlado
		if (++numLogs % NUM_LOGS_TO_FLUSH == 0) {
			logFile.flush();
		}
	}

#if defined(DEBUG)
    // Opcional: Enviar también a la consola de Visual Studio si estás en Debug
    // Muy útil para desarrollo en Xbox 360
    char debugOut[1200];
    sprintf(debugOut, "[%s] %s", ERRLEVELSTXT[level], messageBuffer);
    OutputDebugStringA(debugOut);
#endif
}

// Implementación de los helpers usando la función base
void Logger::debug(const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    char buf[1024]; vsnprintf(buf, 1024, fmt, args);
    write(L_DEBUG, buf);
    va_end(args);
}

void Logger::info(const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    char buf[1024]; vsnprintf(buf, 1024, fmt, args);
    write(L_INFO, buf);
    va_end(args);
}

void Logger::error(const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    char buf[1024]; vsnprintf(buf, 1024, fmt, args);
    write(L_ERROR, buf);
    va_end(args);
}