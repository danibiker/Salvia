#pragma once

#include <SDL.h>

void update_input() {
	gameMenu->joystick->pollKeys(gameMenu->overlay);
	gameMenu->running = !gameMenu->joystick->evento.quit;
}