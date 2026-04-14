#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <cstdarg> // Necesario para va_list
#include <vector>

#define NUM_LOGS_TO_FLUSH 20
#define MAX_MSG_BUFFER_LEN 128

enum {
    L_DEBUG = 0,
    L_INFO,
    L_WARN,
    L_ERROR,
	L_RETRO,
    L_MAX
};

class Logger {
private:
    static unsigned int numLogs;
    static const char* ERRLEVELSTXT[L_MAX];
    static char messageBuffer[MAX_MSG_BUFFER_LEN]; 
	char* logFilepath;
public:
    Logger(const char* filename);
    ~Logger() {
		delete [] logFilepath;
    }
    static void write(int level, const char* fmt, ...);
	static int errorLevel;
};

// Macros para simplificar la llamada a los logs
#define LOG_INFO(fmt, ...)  Logger::write(L_INFO,  fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) Logger::write(L_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  Logger::write(L_WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::write(L_ERROR, "ERROR: " fmt, ##__VA_ARGS__)

//Estos loggers permiten mostrar el fichero y el numero de linea
/*
#define LOG_DEBUG(fmt, ...) Logger::write(L_DEBUG, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  Logger::write(L_WARN,  "[%s:%d] WARNING: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::write(L_ERROR, "[%s:%d] ERROR: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
*/
