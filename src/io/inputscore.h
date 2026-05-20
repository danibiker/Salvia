#pragma once

#include <SDL.h>

extern "C" void salvia_dispatch_keyboard_event(bool down, unsigned retro_keycode,
                                               uint32_t character, uint16_t modifiers);

extern retro_keyboard_event_t core_key_callback;

DWORD WINAPI keyPressSimulated(LPVOID lpParam) {
	t_cap_key* keycap = (t_cap_key*)lpParam;
	const int KEY_UP_TIMEOUT = 50;

	if (core_key_callback) {
		//Si el core ha anunciado un callback, lo llamamos. 
		salvia_dispatch_keyboard_event(true, keycap->retro_key, keycap->character, keycap->retro_mod);
		SDL_Delay(KEY_UP_TIMEOUT);
		salvia_dispatch_keyboard_event(false, keycap->retro_key, keycap->character, keycap->retro_mod);
	} else if (keycap->retro_key < t_joy_state::MAX_RETRO_KEYS && keycap->retro_key != RETROK_UNKNOWN) {
		//Si el core no anuncia callback, se lo indicamos para que lo recoja en el callback definido en
		// salvia.cpp -> retro_input_state
		t_key_input *keyInput    = &gameMenu->joystick->inputs.keyboard_state[keycap->retro_key];
		keyInput->keyjoydown     = true;
		keyInput->key            = keycap->retro_key;
		keyInput->keyMod         = keycap->retro_mod;
		keyInput->unicode        = keycap->character;
		SDL_Delay(KEY_UP_TIMEOUT);
		keyInput->keyjoydown = false;
	}
	delete keycap;
	return 0;
}

void update_input() {
	gameMenu->joystick->pollKeys(gameMenu->overlay);
	gameMenu->running = !gameMenu->joystick->evento.quit;

	if (gameMenu->isOnscreenKeybEnabled()){
		if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_UP)){
			gameMenu->keyb->prevRow();
		} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_DOWN)){
			gameMenu->keyb->nextRow();
		} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_LEFT)){
			gameMenu->keyb->prevCol();
		} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_RIGHT)){
			gameMenu->keyb->nextCol();
		} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_A)){
			t_cap_key *keycap = new t_cap_key(gameMenu->keyb->getSelectedKey());
			HANDLE hThread = CreateThread(NULL, 0, keyPressSimulated, (LPVOID)keycap, CREATE_SUSPENDED, NULL);
			Constant::setup_and_run_thread(hThread, IO_THREAD, true);
		}
	}
}

