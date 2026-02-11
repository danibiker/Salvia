#pragma once

#include <SDL.h>
#include <SDL_joystick.h>

int launchGame(std::string);

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
	} else if(gameMenu.configMenus->getStatus() == EXIT_CONFIG) {
		gameMenu.configMenus->resetStatus();
		SDL_FillRect(gameMenu.video_page, NULL, gameMenu.uBkgColor);
		gameMenu.setEmuStatus(gameMenu.getLastStatus());
	} else if(gameMenu.configMenus->getStatus() == START_SCRAPPING) {
		gameMenu.configMenus->volverMenuInicial();
		SDL_FillRect(gameMenu.video_page, NULL, gameMenu.uBkgColor);
		gameMenu.startScrapping();
	}

	gameMenu.joystick->inputs.clearAll();
	if (optionAction.elem) {
		free(optionAction.elem);
		optionAction.elem = NULL;
	}
	return ret;
}

int processInputs(GameMenu &gameMenu, ListMenu &listMenu, bool generalConfig){
	static Uint32 bkgText = SDL_MapRGB(gameMenu.screen->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);
	int res = 1;

	if (gameMenu.configMenus->getStatus() == POLLING_INPUTS){
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				
				case SDL_QUIT:
					gameMenu.running = false;
					break;
				case SDL_JOYBUTTONDOWN:
					LOG_INFO("Boton detectado: ID %d", (int)event.jbutton.button);
					gameMenu.configMenus->updateButton(event.jbutton.button, KEY_JOY_BTN);
					break;
				case SDL_JOYHATMOTION:
					LOG_INFO("hat detectado: ID %d", (int)event.jhat.value);
					if (event.jhat.value != 0){ //Solo en el momento del joydown
						gameMenu.configMenus->updateButton(event.jhat.value, KEY_JOY_HAT);
                    }
                    break;
				case SDL_JOYAXISMOTION:
					LOG_INFO("axis detectado: value %d axis: %d", (int)event.jaxis.value, (int)event.jaxis.axis);
					gameMenu.configMenus->updateAxis(event.jaxis.value, event.jaxis.axis);
					break;
				default:
					break;
			}
		}
		//gameMenu.joystick->resetAllValues();
	} else {
		gameMenu.joystick->pollKeys(gameMenu.screen);

		if (generalConfig){
			if (gameMenu.joystick->inputs.getAnyTap(0, JOY_BUTTON_UP)){
				gameMenu.configMenus->prevPos();
			} else if (gameMenu.joystick->inputs.getAnyTap(0, JOY_BUTTON_DOWN)){
				gameMenu.configMenus->nextPos();
			}
			
			bool changeInConf = false;

			if (gameMenu.joystick->inputs.getAnyTap(0, JOY_BUTTON_LEFT)){
				gameMenu.configMenus->cambiarValor(-1);
				changeInConf = true;
				if (gameMenu.configMenus->isCoreOptions()){
					gameMenu.configMenus->options_changed_flag = true;
				}
			} else if (gameMenu.joystick->inputs.getAnyTap(0, JOY_BUTTON_RIGHT)){
				changeInConf = true;
				if (gameMenu.configMenus->isCoreOptions()){
					gameMenu.configMenus->options_changed_flag = true;
				}
				gameMenu.configMenus->cambiarValor(1);
			}

			if (gameMenu.joystick->inputs.getBtnTap(0, JOY_BUTTON_A)){
				t_option_action optionAction;
				std::string message = gameMenu.configMenus->confirmar(&optionAction);
				if (processActions(gameMenu, optionAction)){
					return 1;
				}
				if (!message.empty()){
					gameMenu.showSystemMessage(message, 3000);
				}
				changeInConf = true;
			} else if (gameMenu.joystick->inputs.getBtnTap(0, JOY_BUTTON_B)){
				gameMenu.configMenus->volver();
			}

			if (changeInConf || gameMenu.configMenus->options_changed_flag){
				gameMenu.processConfigChanges();
			}

		} else {
			if (gameMenu.joystick->inputs.getAnyTap(0, JOY_BUTTON_UP)){
				listMenu.prevPos();
			} else if (gameMenu.joystick->inputs.getAnyTap(0, JOY_BUTTON_DOWN)){
				listMenu.nextPos();
			} 
			
			if (gameMenu.joystick->inputs.getAnyTap(0, JOY_BUTTON_LEFT)){
				listMenu.prevPage();
			} else if (gameMenu.joystick->inputs.getAnyTap(0, JOY_BUTTON_RIGHT)){
				listMenu.nextPage();
			} 

			

			if (gameMenu.joystick->inputs.getBtnTap(0, JOY_BUTTON_A)){
				vector<string> launchCommand = gameMenu.launchProgram(listMenu);	
				if (launchCommand.size() > 1){
					std::string romToLaunch = launchCommand.at(1);
					LOG_DEBUG("Launching rom %s", romToLaunch.c_str());
					SDL_FillRect(gameMenu.screen, NULL, bkgText);
					if (launchGame(romToLaunch)){
						gameMenu.configMenus->poblarPartidasGuardadas(gameMenu.getCfgLoader(), romToLaunch);
						gameMenu.setEmuStatus(EMU_STARTED);
					}
					return 0;
				}
			}
		}

		if (gameMenu.joystick->inputs.getAnyTap(0, JOY_BUTTON_R)){
			gameMenu.getCfgLoader()->getNextCfgEmu();
			gameMenu.loadEmuCfg(listMenu);
		}
		if (gameMenu.joystick->inputs.getAnyTap(0, JOY_BUTTON_L)){
			gameMenu.getCfgLoader()->getPrevCfgEmu();
			gameMenu.loadEmuCfg(listMenu);
		}

		listMenu.keyUp = gameMenu.joystick->inputs.getAnyReleased(0, JOY_BUTTON_UP) ||
				gameMenu.joystick->inputs.getAnyReleased(0, JOY_BUTTON_DOWN) ||
				gameMenu.joystick->inputs.getAnyReleased(0, JOY_BUTTON_LEFT) ||
				gameMenu.joystick->inputs.getAnyReleased(0, JOY_BUTTON_RIGHT)||
				gameMenu.joystick->inputs.getAnyReleased(0, JOY_BUTTON_L)||
				gameMenu.joystick->inputs.getAnyReleased(0, JOY_BUTTON_R);

		if (HK_VIEW_MENU == gameMenu.joystick->hotkeys->procesarHotkeys(&gameMenu.joystick->inputs)){
			if (gameMenu.getLastStatus() == EMU_STARTED){
				SDL_FillRect(gameMenu.video_page, NULL, gameMenu.uBkgColor);
			}
			gameMenu.setEmuStatus(gameMenu.getLastStatus());
			if (gameMenu.bg_screenshot){
				SDL_FreeSurface(gameMenu.bg_screenshot);
				gameMenu.bg_screenshot = NULL;
			}
			return 0;
		}

		gameMenu.running = !gameMenu.joystick->evento.quit;
	}

	return res;
}

/**
 * 
 */
void updateMenuScreen(TileMap &tileMap, GameMenu &gameMenu, ListMenu &listMenu){
	static Uint32 bkgText = SDL_MapRGB(gameMenu.screen->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);
	ConfigEmu *emu = gameMenu.getCfgLoader()->getCfgEmu();

	if (processInputs(gameMenu, listMenu, emu->generalConfig) == 1){
		if (listMenu.animateBkg) 
			tileMap.draw(gameMenu.video_page);
		else 
			SDL_FillRect(gameMenu.video_page, NULL, bkgText);
		
		gameMenu.refreshScreen(listMenu);
	}
    static uint32_t lastTime = SDL_GetTicks();
    if (SDL_GetTicks() - lastTime > bkgFrameTimeTick && (lastTime = SDL_GetTicks()) > 0){
        tileMap.speed++;
    }
}

void updateMenuOverlay(GameMenu &gameMenu, ListMenu &listMenu){
	static Uint32 bkgText = SDL_MapRGB(gameMenu.screen->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);
	processInputs(gameMenu, listMenu, true);

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
