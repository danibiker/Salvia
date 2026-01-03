#pragma once

#define MAX_PLAYERS 4

// Mapeo conceptual (esto depende de tu mando, aquí un estándar tipo Xbox/PlayStation)
#ifdef _XBOX
enum {
    SDL_BUTTON_A = 0,
    SDL_BUTTON_B = 1,
    SDL_BUTTON_X = 2,
    SDL_BUTTON_Y = 3,
    SDL_BUTTON_L = 4,
    SDL_BUTTON_R = 5,
    SDL_BUTTON_START = 8,
	SDL_BUTTON_SELECT = 9,
	SDL_LEFT_STICK = 10,
	SDL_RIGHT_STICK = 11
};
#elif  defined(WIN)
enum {
    SDL_BUTTON_A = 0,
    SDL_BUTTON_B = 1,
    SDL_BUTTON_X = 2,
    SDL_BUTTON_Y = 3,
    SDL_BUTTON_L = 4,
    SDL_BUTTON_R = 5,
    SDL_BUTTON_SELECT = 6,
    SDL_BUTTON_START = 7,
	SDL_LEFT_STICK = 10,
	SDL_RIGHT_STICK = 11
};
#endif

