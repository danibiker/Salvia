#pragma once
#include "logger.h"

#ifdef _XBOX
    #include <xtl.h>
#else
    #include <windows.h>
#endif
#include <time.h>

// 1. MEMORIA ESTÁTICA: Asegurar que coincida con el nuevo enum (L_MAX)
std::ofstream Logger::logFile;
unsigned int Logger::numLogs = 0;
char Logger::messageBuffer[MAX_MSG_BUFFER_LEN]; 
const char* Logger::ERRLEVELSTXT[L_MAX] = { "DEBUG", "INFO", "WARN", "ERROR", "LIBRETRO" };
int Logger::errorLevel = L_DEBUG;

// 2. SINCRONIZACIÓN: Usar un objeto global real
#ifdef _XBOX
    static CRITICAL_SECTION logSync;
    static bool csInitialized = false;
#endif

Logger::Logger(const char* filename) {
#ifdef _XBOX
    // Inicialización atómica simple para la 360
    if (!csInitialized) {
        InitializeCriticalSection(&logSync);
        csInitialized = true;
    }
#endif

    if (filename && !logFile.is_open()) {
        logFile.open(filename, std::ios::app);
    }
}

void Logger::write(int level, const char* fmt, ...) {
#ifndef DEBUG_LOG
	return;
#endif

    // Validación de seguridad inicial
    if (!fmt || level < errorLevel || level >= L_MAX) return;

#ifdef _XBOX
    EnterCriticalSection(&logSync);
#endif

	char levelStr[25];
	if (level >= L_MAX || level < 0){
		level = L_DEBUG;
	} 

    // 3. TIMESTAMP SEGURO (Específico de Xbox 360 / Win32)
    SYSTEMTIME st;
    char timestamp[32];
    GetLocalTime(&st);
    _snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d", 
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    // 4. FORMATEO DEL MENSAJE
    va_list args;
    va_start(args, fmt);
    int n = _vsnprintf(messageBuffer, sizeof(messageBuffer) - 1, fmt, args);
    va_end(args);
    
    // Forzado de nulo manual
    if (n < 0 || n >= (int)sizeof(messageBuffer) - 1) {
        messageBuffer[sizeof(messageBuffer) - 1] = '\0';
    }

    // 5. SALIDA COMBINADA SEGURA
    char finalBuffer[MAX_MSG_BUFFER_LEN];
    
    // Usamos sizeof(finalBuffer) - 2 para dejar sitio al \n y al \0
    int res = _snprintf(finalBuffer, sizeof(finalBuffer) - 1, "%s", messageBuffer);

    // Si hubo truncado o error, aseguramos el cierre y el salto de línea al final
    if (res < 0 || res >= (int)sizeof(finalBuffer) - 1) {
        finalBuffer[sizeof(finalBuffer) - 1] = '\0';
    }

	strcpy(levelStr, " [");
	strcat(levelStr, ERRLEVELSTXT[level]);
	strcat(levelStr, "] ");

	OutputDebugStringA(timestamp);
	OutputDebugStringA(levelStr);
    OutputDebugStringA(finalBuffer);

	char* ptr = strchr(finalBuffer, '\n');
	if (ptr == nullptr) {
		OutputDebugStringA("\n");
	}

    // Salida a archivo (si está abierto)
    /*if (logFile.is_open()) {
        logFile << finalBuffer;
        logFile.flush(); // Crucial en la 360 si hay un crash, para no perder el log
    }*/

#ifdef _XBOX
    LeaveCriticalSection(&logSync);
#endif
}