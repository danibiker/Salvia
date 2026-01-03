#pragma once

#include <SDL.h>
#include <string>
#include <sstream>
#include "..\utils\logger.h"

static const int video_bpp = 16;
static const int audio_samples = 1024;

#ifdef _XBOX
	static Uint32 video_flags = SDL_SWSURFACE;
	static int video_width = 1280;
	static int video_height = 720;
	static const char *LOG_PATH = "game:\\salvia.log";
#elif  defined(WIN)
	static Uint32 video_flags = SDL_SWSURFACE; //SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN;
	static int video_width = 1280;
	static int video_height = 720;
	static const char *LOG_PATH = "salvia.log";
#endif

class Constant{
	public:
		Constant();
		~Constant();
		// Función auxiliar para convertir int a string (VS2008 no tiene std::to_string)
		static std::string intToString(int value) {
			std::stringstream ss;
			ss << value;
			return ss.str();
		}

		static double round(double number){
			return number < 0.0 ? ceil(number - 0.5) : floor(number + 0.5);
		}

		static Logger *g_Logger;
};

// Macros para simplificar la llamada a los logs
#define LOG_DEBUG(fmt, ...) Constant::g_Logger->write(L_DEBUG, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  Constant::g_Logger->write(L_INFO,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Constant::g_Logger->write(L_ERROR, "[%s:%d] ERROR: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)


