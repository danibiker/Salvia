#pragma once

#include <SDL.h>

void update_input() {
    SDL_Event event;
	bool *joyFrontendStates = gameMenu->joystick->g_joy_frontend_state[0];

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
            case SDL_QUIT:
                gameMenu->running = false;
                break;
			
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP: {
                const int p = event.jbutton.which;
                if (p < MAX_PLAYERS) {
                    const bool pressed = (event.type == SDL_JOYBUTTONDOWN);
					if (event.jbutton.button >= Joystick::buttonsMapperLibretro[p].nButtons)
						break;

					//const int joyFrontendId = Joystick::buttonsMapperFrontend.buttons[event.jbutton.button];
					const int joyFrontendId = event.jbutton.button;
					if (joyFrontendId > -1){
						joyFrontendStates[joyFrontendId] = pressed;
					}

					 const int8_t joyId = Joystick::buttonsMapperLibretro[p].getButton(event.jbutton.button);
					// Solo aplicamos el "latch" al botón START, en la xbox360 el boton start no se detecta, 
					// hasta que se suelta XD
					if (joyId == RETRO_DEVICE_ID_JOYPAD_START) {
						if (pressed) {
							gameMenu->joystick->g_joy_state[p][joyId] = true;
							gameMenu->joystick->startHoldFrames[p] = 0; // Reset por si acaso
						} else {
							// Al soltar, activamos el contador de persistencia
							gameMenu->joystick->startHoldFrames[p] = 3; 
						}
					} else if (joyId >= 0) {
						//LOG_DEBUG("Enviando boton %d para el sdlbtn %d", joyId, event.jbutton.button);
						gameMenu->joystick->g_joy_state[p][joyId] = pressed;
					}
                }
                break;
            }
			
			case SDL_JOYHATMOTION: {
				const int player = event.jhat.which;
				if (player < MAX_PLAYERS) {
					const Uint8 hatVal = event.jhat.value;
					bool* state = gameMenu->joystick->g_joy_state[player];
					t_joy_retro_inputs &input = Joystick::buttonsMapperLibretro[player];

					const int8_t btnUp    = input.getHat(SDL_HAT_UP);
					const int8_t btnDown  = input.getHat(SDL_HAT_DOWN);
					const int8_t btnLeft  = input.getHat(SDL_HAT_LEFT);
					const int8_t btnRight = input.getHat(SDL_HAT_RIGHT);

					if (btnUp >= 0){
						state[btnUp] = (hatVal & SDL_HAT_UP) != 0;
						joyFrontendStates[JOY_BUTTON_UP] = state[btnUp];
					}
					if (btnDown >= 0){
						state[btnDown] = (hatVal & SDL_HAT_DOWN) != 0;
						joyFrontendStates[JOY_BUTTON_DOWN] = state[btnDown];
					}
					if (btnLeft >= 0) {
						state[btnLeft] = (hatVal & SDL_HAT_LEFT) != 0;
						joyFrontendStates[JOY_BUTTON_LEFT] = state[btnLeft];
					}
					if (btnRight >= 0) {
						state[btnRight] = (hatVal & SDL_HAT_RIGHT) != 0;
						joyFrontendStates[JOY_BUTTON_RIGHT] = state[btnRight];
					}
					//LOG_DEBUG("SDL_JOYHATMOTION: %d%d%d%d", state[btnUp], state[btnDown], state[btnLeft], state[btnRight]);
				}
				break;
			}
					   
		   case SDL_JOYAXISMOTION: {
				const int p = event.jaxis.which;
				if (p < MAX_PLAYERS && gameMenu->joystick->buttonsMapperLibretro[p].axisAsPad) {
					bool* axisState = gameMenu->joystick->g_axis_state[p];
					int8_t targetNeg = Joystick::buttonsMapperLibretro[p].getAxis(event.jaxis.axis * 2);
					int8_t targetPos = Joystick::buttonsMapperLibretro[p].getAxis((event.jaxis.axis * 2) + 1);

					if (event.jaxis.value > DEADZONE) {
						if (targetPos >= 0) axisState[targetPos] = true;
						if (targetNeg >= 0) axisState[targetNeg] = false;
					} else if (event.jaxis.value < -DEADZONE) {
						if (targetNeg >= 0) axisState[targetNeg] = true;
						if (targetPos >= 0) axisState[targetPos] = false;
					} else {
						if (targetPos >= 0) axisState[targetPos] = false;
						if (targetNeg >= 0) axisState[targetNeg] = false;
					}
				}
				break;
			}

			case SDL_KEYUP: {
				if (event.key.keysym.sym == SDLK_F6){
					saveState();
				} else if (event.key.keysym.sym == SDLK_F9){
					loadState();
				} else if (event.key.keysym.sym == SDLK_BACKSPACE){
					*gameMenu->current_sync = gameMenu->sync->g_sync_last;
					SDL_PauseAudio(0);
				}
				break;
			} 
			case SDL_KEYDOWN: {
				if (event.key.keysym.sym == SDLK_BACKSPACE){
					gameMenu->sync->g_sync_last = *gameMenu->current_sync;
					*gameMenu->current_sync = SYNC_NONE;
					SDL_PauseAudio(1);
				}
				break;
			}
		}
	}

	//const Uint32 now = SDL_GetTicks();
	//LOG_DEBUG("ticks are: %d\n", SDL_GetTicks() - gameMenu->joystick->lastSelectPress);
	/*if (gameMenu->joystick->lastSelectPress > 0 && now - gameMenu->joystick->lastSelectPress > LONGKEYTIMEOUT){
		gameMenu->joystick->lastSelectPress = 0;
		gameMenu->setEmuStatus(EMU_MENU);
		gameMenu->joystick->resetAllValues();
	}*/
}