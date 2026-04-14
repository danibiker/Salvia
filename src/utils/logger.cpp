#pragma once
#include "logger.h"
#include <io/fileio.h>

#ifdef _XBOX
    #include <xtl.h>
#else
    #include <windows.h>
#endif
#include <time.h>


unsigned int Logger::numLogs = 0;
char Logger::messageBuffer[MAX_MSG_BUFFER_LEN]; 
const char* Logger::ERRLEVELSTXT[L_MAX] = { "DEBUG", "INFO", "WARN", "ERROR", "LIBRETRO" };
int Logger::errorLevel = L_DEBUG;

#ifdef _XBOX
    static CRITICAL_SECTION logSync;
    static bool csInitialized = false;
#endif

Logger::Logger(const char* filename) {
	#ifdef _XBOX
    if (!csInitialized) {
        InitializeCriticalSection(&logSync);
        csInitialized = true;
    }
	#endif
	size_t len = strlen(filename);
	logFilepath = new char[len + 1]	;
	strcpy_s(logFilepath, len + 1, filename);
}

void Logger::write(int level, const char* fmt, ...) {
	#ifndef DEBUG_LOG
	return;
	#endif
    if (!fmt || level < errorLevel || level >= L_MAX) return;

	#ifdef _XBOX
    EnterCriticalSection(&logSync);
	#endif

	char levelStr[25];
	if (level >= L_MAX || level < 0){
		level = L_DEBUG;
	} 

    SYSTEMTIME st;
    char timestamp[32];
    GetLocalTime(&st);
    _snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d", 
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    va_list args;
    va_start(args, fmt);
	int n = _vsnprintf_s(messageBuffer, MAX_MSG_BUFFER_LEN, _TRUNCATE, fmt, args);
    va_end(args);
    
    if (n < 0 || n >= MAX_MSG_BUFFER_LEN - 1) {
        messageBuffer[MAX_MSG_BUFFER_LEN - 1] = '\0';
    }

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

	/*Fileio fileio;
	fileio.writeToFile(logFilepath, finalBuffer, strlen(finalBuffer), 1);*/

	#ifdef _XBOX
    LeaveCriticalSection(&logSync);
	#endif
}