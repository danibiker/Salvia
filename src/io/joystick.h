#pragma once

#include <SDL.h>
#include "..\libretro.h"

#define MAX_PLAYERS 4
SDL_Joystick* g_joysticks[MAX_PLAYERS] = { NULL };
bool g_joy_state[MAX_PLAYERS][RETRO_DEVICE_ID_JOYPAD_R3 + 1] = { false };

// Mapeo conceptual (esto depende de tu mando, aquí un estándar tipo Xbox/PlayStation)
enum {
    SDL_BUTTON_A = 0,
    SDL_BUTTON_B = 1,
    SDL_BUTTON_X = 2,
    SDL_BUTTON_Y = 3,
    SDL_BUTTON_L = 4,
    SDL_BUTTON_R = 5,
    SDL_BUTTON_SELECT = 6,
    SDL_BUTTON_START = 7
};

