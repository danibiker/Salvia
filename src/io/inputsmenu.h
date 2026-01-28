#pragma once

#include <SDL.h>

int launchGame(std::string);

/**
 * 
 */
void updateMenuScreen(TileMap &tileMap, GameMenu &gameMenu, ListMenu &listMenu, bool keypress){
	static Uint32 bkgText = SDL_MapRGB(gameMenu.screen->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);
	if (gameMenu.configMenus->getStatus() == POLLING_INPUTS){
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_QUIT:
					gameMenu.running = false;
					break;
				case SDL_JOYBUTTONDOWN:
					gameMenu.configMenus->updateButton(event.jbutton.button, KEY_JOY_BTN);
					break;
				case SDL_JOYHATMOTION:
					if (event.jhat.value != 0){ //Solo en el momento del joydown
						gameMenu.configMenus->updateButton(event.jhat.value, KEY_JOY_HAT);
                    }
                    break;
				case SDL_JOYAXISMOTION:
					gameMenu.configMenus->updateAxis(event.jaxis.value, event.jaxis.axis);
					break;
				default:
					break;
			}
		}
		gameMenu.joystick->resetAllValues();
		//gameMenu.joystick->lastSelectPress = 0;
	} else {
		tEvento askEvento;
		//Procesamos los controles de la aplicacion
		askEvento = gameMenu.WaitForKey();
		bool *joyFrontendStates = gameMenu.joystick->g_joy_frontend_state[0];

		if (askEvento.isJoy && askEvento.keyjoydown){
			ConfigEmu *emu = gameMenu.getCfgLoader()->getCfgEmu();

			if (listMenu.getNumGames() == 0 && emu->generalConfig){
				switch(askEvento.joy){
					case JOY_BUTTON_UP:{
						gameMenu.configMenus->prevPos();
						break;
					}
					case JOY_BUTTON_DOWN:{
						gameMenu.configMenus->nextPos();
						break;
					}
					case JOY_BUTTON_LEFT:{
						gameMenu.configMenus->cambiarValor(-1);
						if (gameMenu.configMenus->isCoreOptions()){
							gameMenu.configMenus->options_changed_flag = true;
							/*Esta es la técnica que usan los frontends como RetroArch para previsualizar cambios en menús pausados:
							Realiza un retro_serialize() para guardar el estado actual en memoria.
							Aplica el cambio de variable.
							Llama a retro_run() una vez para que el core procese el cambio y genere el nuevo frame.
							Inmediatamente después, llama a retro_unserialize() para volver al estado anterior. 
							Resultado: El usuario ve el frame actualizado con la nueva configuración, pero lógicamente el juego sigue en el mismo punto exacto.*/
						}
						break;
					}
					case JOY_BUTTON_RIGHT:{
						if (gameMenu.configMenus->isCoreOptions()){
							gameMenu.configMenus->options_changed_flag = true;
						}
						gameMenu.configMenus->cambiarValor(1);
						break;
					}
					case JOY_BUTTON_A:{
						if (!askEvento.keyjoydown) break;
						t_option_action optionAction;

						std::string message = gameMenu.configMenus->confirmar(&optionAction);
						if (!message.empty()){
							gameMenu.showSystemMessage(message, 3000);
						}
						break;
					}
					case JOY_BUTTON_B:{
						if (!askEvento.keyjoydown) break;
						gameMenu.configMenus->volver();
						break;
					}
					default:
						break;
				}

				if (askEvento.joy == JOY_BUTTON_A || askEvento.joy == JOY_BUTTON_LEFT
					|| askEvento.joy == JOY_BUTTON_RIGHT){
					gameMenu.processConfigChanges();
				}

			} else {
				//Opciones para seleccionar una rom para el emulador
				if (askEvento.joy == JOY_BUTTON_UP){
					listMenu.prevPos();
				} else if (askEvento.joy == JOY_BUTTON_DOWN){
					listMenu.nextPos();
				} else if (askEvento.joy == JOY_BUTTON_LEFT){
					listMenu.prevPage();
				} else if (askEvento.joy == JOY_BUTTON_RIGHT){
					listMenu.nextPage();
				} 

				if (askEvento.joy == JOY_BUTTON_A){
					vector<string> launchCommand = gameMenu.launchProgram(listMenu);
					
					if (launchCommand.size() > 1){
						std::string romToLaunch = launchCommand.at(1);
						LOG_DEBUG("Launching rom %s", romToLaunch.c_str());

						SDL_FillRect(gameMenu.screen, NULL, bkgText);
						if (launchGame(romToLaunch)){
							gameMenu.configMenus->poblarPartidasGuardadas(gameMenu.getCfgLoader(), romToLaunch);
							gameMenu.setEmuStatus(EMU_STARTED);
						}
						gameMenu.joystick->resetAllValues();
						//gameMenu.joystick->lastSelectPress = 0;
						return;
					}
				} 
			}

			if (askEvento.joy == JOY_BUTTON_R){
				//Change to prev emulator
				//sound.play(SBTNCLICK);
				//gameMenu.showMessage("Refreshing gamelist...");
				gameMenu.getCfgLoader()->getNextCfgEmu();
				gameMenu.loadEmuCfg(listMenu);
			}
			if (askEvento.joy == JOY_BUTTON_L){
				//Change to next emulator
				//sound.play(SBTNCLICK);
				//gameMenu.showMessage("Refreshing gamelist...");
				gameMenu.getCfgLoader()->getPrevCfgEmu();
				gameMenu.loadEmuCfg(listMenu);
			} 
		}

		//To detect hotkeys
		if (askEvento.joy > -1){
			joyFrontendStates[askEvento.joy] = askEvento.keyjoydown;
			const int keyMenu = gameMenu.joystick->hotkeys.getTriggerForAction(HK_VIEW_MENU);
			if (keyMenu > -1){
				if (joyFrontendStates[keyMenu] && joyFrontendStates[gameMenu.joystick->hotkeys.g_modifierButton] && gameMenu.getLastStatus() == EMU_STARTED){
					SDL_FillRect(gameMenu.video_page, NULL, gameMenu.uBkgColor);
					gameMenu.setEmuStatus(EMU_STARTED);
					gameMenu.joystick->resetButtonsFrontend();
					return;
				}
			}
		}

		if (askEvento.quit){
			gameMenu.running = false; // Marcamos para salir
		}
	}

    if (listMenu.animateBkg) 
		tileMap.draw(gameMenu.video_page);
    else 
		SDL_FillRect(gameMenu.video_page, NULL, bkgText);

    gameMenu.refreshScreen(listMenu);

    static uint32_t lastTime = SDL_GetTicks();
    if (SDL_GetTicks() - lastTime > bkgFrameTimeTick && (lastTime = SDL_GetTicks()) > 0){
        tileMap.speed++;
    }
}

/**
*
*/
bool processActions(GameMenu &gameMenu, t_option_action &optionAction){
	bool ret = false;	
	if (optionAction.option == OPC_SAVESTATE){
		const char *filepath = (const char *) optionAction.elem;
		int iPosSlot = optionAction.indexSelected;
		dirutil dir;

		if (iPosSlot >= 0 && iPosSlot < MAX_SAVESTATES) {
			switch(optionAction.action){
				case ASK_CARGAR:
					LOG_DEBUG("Peticion cargar Partida: %s", filepath);
					g_currentSlot = iPosSlot;
					loadState();
					gameMenu.setEmuStatus(EMU_STARTED);
					gameMenu.configMenus->resetStatus();
					ret = true;
					break;
				case ASK_GUARDAR:
					LOG_DEBUG("Peticion guardar Partida: %s", filepath);
					g_currentSlot = iPosSlot;
					action_postponed.cycles = 1;
					action_postponed.action = HK_SAVESTATE;
					gameMenu.setEmuStatus(EMU_STARTED);
					gameMenu.configMenus->resetStatus();
					break;
				case ASK_ELIMINAR:
					LOG_DEBUG("Peticion eliminar Partida: %s", filepath);
					dir.borrarArchivo(filepath);
					dir.borrarArchivo(std::string(filepath) + ".png");
					gameMenu.configMenus->resetStatus();
					gameMenu.configMenus->poblarPartidasGuardadas(gameMenu.getCfgLoader(), gameMenu.getRomPaths()->rompath);
					break;
			}
		}
	}

	gameMenu.joystick->resetAllValues();
	if (optionAction.elem) {
		free(optionAction.elem);
		optionAction.elem = NULL;
	}
	return ret;
}

void processEvents(GameMenu &gameMenu, ListMenu &listMenu, tEvento &askEvento){
	bool *joyFrontendStates = gameMenu.joystick->g_joy_frontend_state[0];

	switch(askEvento.joy){
		case JOY_BUTTON_UP:{
			gameMenu.configMenus->prevPos();
			break;
		}
		case JOY_BUTTON_DOWN:{
			gameMenu.configMenus->nextPos();
			break;
		}
		case JOY_BUTTON_LEFT:{
			gameMenu.configMenus->cambiarValor(-1);
			if (gameMenu.configMenus->isCoreOptions()){
				gameMenu.configMenus->options_changed_flag = true;
				/*Esta es la técnica que usan los frontends como RetroArch para previsualizar cambios en menús pausados:
				Realiza un retro_serialize() para guardar el estado actual en memoria.
				Aplica el cambio de variable.
				Llama a retro_run() una vez para que el core procese el cambio y genere el nuevo frame.
				Inmediatamente después, llama a retro_unserialize() para volver al estado anterior. 
				Resultado: El usuario ve el frame actualizado con la nueva configuración, pero lógicamente el juego sigue en el mismo punto exacto.*/
			}
			break;
		}
		case JOY_BUTTON_RIGHT:{
			if (gameMenu.configMenus->isCoreOptions()){
				gameMenu.configMenus->options_changed_flag = true;
			}
			gameMenu.configMenus->cambiarValor(1);
			break;
		}
		case JOY_BUTTON_A:{
			if (!askEvento.keyjoydown) break;
			t_option_action optionAction;
			std::string message = gameMenu.configMenus->confirmar(&optionAction);
			if (processActions(gameMenu, optionAction)){
				return;
			}
			if (!message.empty()){
				gameMenu.showSystemMessage(message, 3000);
			}		
			break;
		}
		case JOY_BUTTON_B:{
			if (!askEvento.keyjoydown) break;
			gameMenu.configMenus->volver();
			break;
		}
		default:
			break;
	}

	if (askEvento.joy == JOY_BUTTON_A || askEvento.joy == JOY_BUTTON_LEFT
		|| askEvento.joy == JOY_BUTTON_RIGHT){
		gameMenu.processConfigChanges();
	}

	//To detect hotkeys
	if (askEvento.joy > -1){
		joyFrontendStates[askEvento.joy] = askEvento.keyjoydown;
		const int keyMenu = gameMenu.joystick->hotkeys.getTriggerForAction(HK_VIEW_MENU);
		if (keyMenu > -1){
			if (joyFrontendStates[keyMenu] && joyFrontendStates[gameMenu.joystick->hotkeys.g_modifierButton] && gameMenu.getLastStatus() == EMU_STARTED){
				gameMenu.setEmuStatus(EMU_STARTED);
				gameMenu.joystick->resetButtonsFrontend();
				if (gameMenu.bg_screenshot){
					SDL_FreeSurface(gameMenu.bg_screenshot);
					gameMenu.bg_screenshot = NULL;
				}
				return;
			}
		}
	}

	if (askEvento.quit){
		gameMenu.running = false; // Marcamos para salir
	}
}

void pollInputs(GameMenu &gameMenu, ListMenu &listMenu){
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				gameMenu.running = false;
				break;
			case SDL_JOYBUTTONDOWN:
				gameMenu.configMenus->updateButton(event.jbutton.button, KEY_JOY_BTN);
				break;
			case SDL_JOYHATMOTION:
				if (event.jhat.value != 0){ //Solo en el momento del joydown
					gameMenu.configMenus->updateButton(event.jhat.value, KEY_JOY_HAT);
                }
                break;
			case SDL_JOYAXISMOTION:
				gameMenu.configMenus->updateAxis(event.jaxis.value, event.jaxis.axis);
				break;
			default:
				break;
		}
	}
	gameMenu.joystick->resetAllValues();
}

void updateMenuOverlay(GameMenu &gameMenu, ListMenu &listMenu){
	static Uint32 bkgText = SDL_MapRGB(gameMenu.screen->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);

	if (gameMenu.configMenus->getStatus() == POLLING_INPUTS){
		pollInputs(gameMenu, listMenu);
	} else {	
		//Procesamos los controles de la aplicacion
		tEvento askEvento = gameMenu.WaitForKey();
		processEvents(gameMenu, listMenu, askEvento);
	}

	if (gameMenu.bg_screenshot){
		SDL_BlitSurface(gameMenu.bg_screenshot, NULL, gameMenu.video_page, NULL);
	} else {
		SDL_FillRect(gameMenu.video_page, NULL, bkgText);
	}
	
	if (gameMenu.getEmuStatus() == EMU_MENU_OVERLAY){
		//Dibujamos el menu
		gameMenu.configMenus->draw(gameMenu.video_page);
	}
}	
