#pragma once

#include <string>
#include <SDL.h>

static int video_bpp = 16;

#ifdef _XBOX
	static Uint32 video_flags = SDL_SWSURFACE;
	static int video_width = 1280;
	static int video_height = 720;
#elif  defined(WIN)
	static Uint32 video_flags = SDL_SWSURFACE; //SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN;
	static int video_width = 1280;
	static int video_height = 720;
#endif

static int audio_samples = 1024;


// Función auxiliar para convertir int a string (VS2008 no tiene std::to_string)
static std::string intToString(int value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

double round(double number)
{
    return number < 0.0 ? ceil(number - 0.5) : floor(number + 0.5);
}
