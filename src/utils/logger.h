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
    static std::ofstream logFile;
    static unsigned int numLogs;
    static const char* ERRLEVELSTXT[L_MAX];
	// Buffer estático para no saturar la pila de la Xbox 360
    static char messageBuffer[2048]; 
public:
    Logger(const char* filename);

    ~Logger() {
        close();
    }
	// Función principal con soporte de formato printf
    static void write(int level, const char* fmt, ...);

	static void close(){
		if (logFile.is_open()) {
            logFile.flush();
            logFile.close();
        }
	}
	
	static int errorLevel;
    
};

// Macros para simplificar la llamada a los logs
//#ifdef DEBUG
	#define LOG_DEBUG(fmt, ...) Logger::write(L_DEBUG, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
	#define LOG_INFO(fmt, ...)  Logger::write(L_INFO,  fmt, ##__VA_ARGS__)
	#define LOG_ERROR(fmt, ...) Logger::write(L_ERROR, "[%s:%d] ERROR: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
/*#else
	#define LOG_DEBUG(fmt, ...) do { } while (0)
    #define LOG_INFO(fmt, ...)  do { } while (0)
    #define LOG_ERROR(fmt, ...) do { } while (0)
#endif*/

