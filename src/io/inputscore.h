#pragma once

#include <SDL.h>

extern "C" void salvia_dispatch_keyboard_event(bool down, unsigned retro_keycode,
                                               uint32_t character, uint16_t modifiers);

extern retro_keyboard_event_t core_key_callback;

/* Cuanto se mantiene "pulsada" una tecla del teclado en pantalla. */
static const int KEY_UP_TIMEOUT = 50;

/* Suelta la tecla simulada cuando vence su plazo. Se llama desde update_input(), o
 * sea en el hilo principal: sin hilo aparte, no hay carrera con quien lee
 * keyboard_state, y el evento del core se despacha desde su hilo de siempre. */
static void releaseSimulatedKey() {
	t_joy_state &in = gameMenu->joystick->inputs;
	if (!in.simKey.active) return;
	if (SDL_GetTicks() < in.simKey.releaseAt) return;

	if (core_key_callback) {
		salvia_dispatch_keyboard_event(false, in.simKey.retro_key, in.simKey.character, in.simKey.retro_mod);
	} else if (in.simKey.retro_key < t_joy_state::MAX_RETRO_KEYS) {
		in.keyboard_state[in.simKey.retro_key].keyjoydown = false;
	}
	in.simKey.active = false;
}

static void pressSimulatedKey(const t_cap_key &keycap) {
	t_joy_state &in = gameMenu->joystick->inputs;

	/* Si quedaba una pendiente, se suelta antes de pisarla. */
	if (in.simKey.active) {
		in.simKey.releaseAt = 0;
		releaseSimulatedKey();
	}

	if (core_key_callback) {
		//Si el core ha anunciado un callback, lo llamamos.
		salvia_dispatch_keyboard_event(true, keycap.retro_key, keycap.character, keycap.retro_mod);
	} else if (keycap.retro_key < t_joy_state::MAX_RETRO_KEYS && keycap.retro_key != RETROK_UNKNOWN) {
		//Si el core no anuncia callback, se lo indicamos para que lo recoja en el callback definido en
		// salvia.cpp -> retro_input_state
		t_key_input *keyInput    = &in.keyboard_state[keycap.retro_key];
		keyInput->keyjoydown     = true;
		keyInput->key            = keycap.retro_key;
		keyInput->keyMod         = keycap.retro_mod;
		keyInput->unicode        = keycap.character;
	} else {
		return;   /* tecla no representable: no hay nada que soltar despues */
	}

	in.simKey.retro_key = keycap.retro_key;
	in.simKey.retro_mod = keycap.retro_mod;
	in.simKey.character = keycap.character;
	in.simKey.releaseAt = SDL_GetTicks() + KEY_UP_TIMEOUT;
	in.simKey.active    = true;
}

void update_input() {
	gameMenu->joystick->pollKeys(gameMenu->getEmuStatus());
	gameMenu->running = !gameMenu->joystick->evento.quit;

	/* Fuera del 'if': si se cierra el teclado en pantalla justo despues de pulsar,
	 * la tecla tiene que soltarse igual o se queda enganchada. */
	releaseSimulatedKey();

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
			pressSimulatedKey(gameMenu->keyb->getSelectedKey());
		}
	}
}

