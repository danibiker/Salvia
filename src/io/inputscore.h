#pragma once

#include <SDL.h>

void update_input() {
	gameMenu->joystick->pollKeys(gameMenu->screen);
	gameMenu->running = !gameMenu->joystick->evento.quit;
}