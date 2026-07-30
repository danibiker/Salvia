#pragma once

#include <SDL.h>
#include <SDL_joystick.h>
#include <io/keyboard.h>
#include <video/HLSLBackground.h>

#ifdef WIN
	#include <video/win_d3d9.h>
#endif

#ifdef _XBOX
	extern "C" struct IDirect3DDevice9* D3D_Device;
#endif

extern int launchGame(std::string rompath, bool tmpDelete=true);
extern t_rom_paths romPaths;
std::vector<struct MenuStatus> historyNavigation;

/**
*
*/
bool processActions(GameMenu*& gameMenu, t_option_action &optionAction){
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
					gameMenu->setEmuStatus(EMU_STARTED);
					gameMenu->configMenus->resetStatus();
					gameMenu->clearOverlay();
					ret = true;
					break;
				case ASK_GUARDAR:
					LOG_DEBUG("Peticion guardar Partida: %s", filepath);
					g_currentSlot = iPosSlot;
					action_postponed.cycles = 1;
					action_postponed.action = SAVE_STATE;
					gameMenu->setEmuStatus(EMU_STARTED);
					gameMenu->configMenus->resetStatus();
					gameMenu->clearOverlay();
					break;
				case ASK_ELIMINAR:
					LOG_DEBUG("Peticion eliminar Partida: %s", filepath);
					dir.borrarArchivo(filepath);
					dir.borrarArchivo(std::string(filepath) + ".png");
					gameMenu->configMenus->resetStatus();
					gameMenu->configMenus->poblarPartidasGuardadas(gameMenu->getCfgLoader(), romPaths.rompath);
					break;
			}
		}
	} else if(gameMenu->configMenus->getStatus() == EXIT_CONFIG) {
		gameMenu->configMenus->resetStatus();
		gameMenu->clearOverlay();
		gameMenu->setEmuStatus(gameMenu->getLastStatus());
	} else if(gameMenu->configMenus->getStatus() == START_SCRAPPING) {
		gameMenu->configMenus->volverMenuInicial();
		gameMenu->clearOverlay();
		gameMenu->startScrapping();
	} else if (gameMenu->configMenus->getStatus() == EXIT_EMULATION){
		gameMenu->running = false;
	} 

	gameMenu->joystick->inputs.clearAll();
	if (optionAction.elem) {
		free(optionAction.elem);
		optionAction.elem = NULL;
	}
	return ret;
}

inline void procesarOverlayTeclado(){
	if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_UP)){
		gameMenu->keyb->prevRow();
	} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_DOWN)){
		gameMenu->keyb->nextRow();
	} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_LEFT)){
		gameMenu->keyb->prevCol();
	} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_RIGHT)){
		gameMenu->keyb->nextCol();
	}
}

static void popBackUtf8(std::string& s) {
    if (s.empty()) return;
    std::size_t pos = s.size() - 1;
    while (pos > 0 && (s[pos] & 0xC0) == 0x80)
        pos--;  // saltar bytes de continuacion
    s.erase(pos);
}

inline int getPressedButton(){
	int pressedButton = -1;

	// 1. OBTENER ESTADOS DE LOS BOTONES CARDINALES
	bool up    = gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_UP);
	bool down  = gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_DOWN);
	bool left  = gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_LEFT);
	bool right = gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_RIGHT);

	// 2. DETECTAR DIAGONALES PRIMERO (Combinación de dos ejes)
	if (up && left)          pressedButton = JOY_BUTTON_UPLEFT;
	else if (up && right)     pressedButton = JOY_BUTTON_UPRIGHT;
	else if (down && left)    pressedButton = JOY_BUTTON_DOWNLEFT;
	else if (down && right)   pressedButton = JOY_BUTTON_DOWNRIGHT;
    
	// 3. DETECTAR CARDINALES SI NO HUBO DIAGONAL (Un solo eje)
	else if (up)              pressedButton = JOY_BUTTON_UP;
	else if (down)            pressedButton = JOY_BUTTON_DOWN;
	else if (left)            pressedButton = JOY_BUTTON_LEFT;
	else if (right)           pressedButton = JOY_BUTTON_RIGHT;

	return pressedButton;
}

inline int procesarGeneralConfig(){
	bool changeInConf = false;
	const TipoOpcion type = gameMenu->configMenus->getMenuType();
	
	if (type == OPC_SHOW_IMG && !gameMenu->configMenus->imageFaq.isTamAuto()){
		//Handling FAQ image movement
		gameMenu->configMenus->imageFaq.softMove(gameMenu->gameTicks.dt, getPressedButton());
	} else if (type == OPC_FAQ_SEARCH || type == OPC_FAQ_SELECT || type == OPC_FAQ_TXT || type == OPC_SHOW_TXT){
		//Handling non modificable menu options
		if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_UP)){
			gameMenu->configMenus->prevPos();
		} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_DOWN)){
			gameMenu->configMenus->nextPos();
		} 
		if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_LEFT)){
			gameMenu->configMenus->prevPage();
		} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_RIGHT)){
			gameMenu->configMenus->nextPage();
		}
	} else {
		//Handling modificable menu options
		if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_UP)){
			gameMenu->configMenus->prevPos();
		} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_DOWN)){
			gameMenu->configMenus->nextPos();
		}

		if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_LEFT)){
			gameMenu->configMenus->cambiarValor(-1);
			changeInConf = true;
			if (gameMenu->configMenus->isCoreOptions()){
				gameMenu->configMenus->options_changed_flag = true;
			}
		} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_RIGHT)){
			changeInConf = true;
			if (gameMenu->configMenus->isCoreOptions()){
				gameMenu->configMenus->options_changed_flag = true;
			}
			gameMenu->configMenus->cambiarValor(1);
		}
	}

	if (gameMenu->joystick->inputs.getBtnTap(0, JOY_BUTTON_A)){
		t_option_action optionAction;
		std::string message = gameMenu->configMenus->confirmar(&optionAction);
		if (processActions(gameMenu, optionAction)){
			return 1;
		}
		if (!message.empty()){
			gameMenu->showSystemMessage(message, 3000);
		}
		changeInConf = true;
	} else if (gameMenu->joystick->inputs.getBtnTap(0, JOY_BUTTON_B)){
		gameMenu->configMenus->volver();
	}

	int& retro_key = gameMenu->joystick->inputs.last_key_processed->key;
	if (retro_key != -1){
		t_key_input *keyInput = &gameMenu->joystick->inputs.keyboard_state[retro_key];
		LOG_DEBUG("keyInput->keyjoydown: %d, %d, %d->%c", keyInput->key, keyInput->keyMod, keyInput->unicode, keyInput->unicode);
		//Reset the last key pressed
		retro_key = -1;

		Menu* menuActual = gameMenu->configMenus->obtenerMenuActual();
		if ((int)menuActual->opciones.size() > menuActual->seleccionado && menuActual->opciones[menuActual->seleccionado]->tipo == OPC_SHOW_TXT_VAL){
			OpcionTxtAndValue *OptSel = static_cast<OpcionTxtAndValue *>(menuActual->opciones[menuActual->seleccionado]);

			if (!OptSel->editable){
				return 0;
			}
			
			if (keyInput->key == RETROK_RETURN){
				//Actualizamos el valor realmente en CFG::
				if (OptSel->cfgKey < cfg::MAIN_CFG_MAX){
					CfgLoader::configMain[OptSel->cfgKey].setPropValue(OptSel->tmpValue);
				} 
				//Volvemos a dejar el valor ofuscado si aplica
				if (OptSel->isPassword)
					OptSel->valor = PASS_MASK;
			} else if (keyInput->key == RETROK_DELETE){
				OptSel->tmpValue.clear();
				OptSel->valor = OptSel->tmpValue;
				LOG_DEBUG("Modificando valor del menu");
			} else if (keyInput->key == RETROK_BACKSPACE){
				if (!OptSel->valor.empty()){
					popBackUtf8(OptSel->tmpValue);
				}
				OptSel->valor = OptSel->tmpValue;
				LOG_DEBUG("Modificando valor del menu");
			} else if (keyInput->key >= RETROK_SPACE && keyInput->unicode > 0){
				OptSel->tmpValue += Constant::unicodeToUtf8String(keyInput->unicode);
				OptSel->valor = OptSel->tmpValue;
				LOG_DEBUG("Modificando valor del menu");
			}
		}
	}

	if (changeInConf || gameMenu->configMenus->options_changed_flag){
		gameMenu->processConfigChanges();
	}

	return 0;
}

void restoreHistory(ListMenu &listMenu){
	if (!historyNavigation.empty()){
		MenuStatus ms = historyNavigation.back();
		historyNavigation.pop_back();
		listMenu.iniPos = ms.iniPos;
		listMenu.endPos = ms.endPos;
		listMenu.curPos = ms.curPos;
	} else {
		listMenu.resetIndexPos();
	}
}

int procesarAccionesMenu(ListMenu &listMenu){
	if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_UP)){
		listMenu.prevPos();
	} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_DOWN)){
		listMenu.nextPos();
	} 
			
	if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_LEFT)){
		listMenu.prevPage();
	} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_RIGHT)){
		listMenu.nextPage();
	} 

	if (gameMenu->joystick->inputs.getBtnTap(0, JOY_BUTTON_B)){
		if (listMenu.listZipped.cdBack()){
			gameMenu->listableZip(listMenu, FS_ZIP_CD_BACK);
		} else if (listMenu.listDir.cdBack()){
			gameMenu->listableDir(listMenu, FS_DIR_BACK);
		} else {
			gameMenu->loadEmuCfg(listMenu);
			listMenu.listZipped.clear();
			//Reseteamos el tamanyo de la lista para no mostrar la ruta relativa
			//en el caso de que estuviesemos mostrando el contenido de un directorio
			//o un fichero comprimido
			listMenu.setLayout(LAYBOXES, gameMenu->overlay->w, gameMenu->overlay->h);
		}
		restoreHistory(listMenu);
	}

	if (gameMenu->joystick->inputs.getBtnTap(0, JOY_BUTTON_A)){
		if ((std::size_t)listMenu.curPos >= listMenu.filteredGames.size()){
			LOG_ERROR("List is empty or position is wrong");
			return 1;
		}

		//Saving the position
		gameMenu->saveGameMenuPos(listMenu);
		//Save the actual positions to the history to be able to step back and highlight the proper menu
		MenuStatus ms = {listMenu.iniPos, listMenu.endPos, listMenu.curPos, 0, 0, 0, NULL};

		std::string romToLaunch;
		bool deleteTmpDir = true;
		auto& game = listMenu.filteredGames.at(listMenu.curPos);
		ConfigEmu* emu = gameMenu->getCfgLoader()->getCfgEmu();
		
		//Call to listableZip to load a zip with games inside. The name of the file 
		//should start with the character "@"
		FILE_STATUS fs = gameMenu->listableZip(listMenu, FS_ZIP_CD);
		
		if (fs == FS_ZIP_NAVIGATION){
			//Case navigating inside the zip structure	
			historyNavigation.push_back(ms);
			return 1;
		} else if (fs == FS_ZIP_EXTRACT_ERROR){
			//Case for extraction errors
			LOG_ERROR("Error extracting rom from zip file %s", listMenu.listZipped.file.c_str());
			return 1;
		} else if (fs == FS_ZIP_FILE_EXTRACTED){
			//Case when a file is succesfully extracted onto the filesystem
			LOG_DEBUG("Launching extracted rom %s", listMenu.listZipped.extractedFile.c_str());
			romToLaunch = listMenu.listZipped.extractedFile;
			deleteTmpDir = false;
		} else {
			//It's not a zip file. Try to list if it's a directory
			FILE_STATUS fs = gameMenu->listableDir(listMenu, FS_DIR_CD);
			if (fs == FS_DIR_ISFILE){
				romToLaunch = listMenu.listDir.dir;
				std::string relativePath = listMenu.listDir.getRelativePath();
				if (!relativePath.empty()){
					romToLaunch.append(relativePath + Constant::getFileSep());
				}
				romToLaunch.append(listMenu.listDir.file);
			} else {
				//Navigating the directory
				historyNavigation.push_back(ms);
				return 1;
			}
		}

		//Launch the game with the proper emulator if the actual is not correct
		gameMenu->launchProgram(romToLaunch);
		//Launch the game if the actual emulator is correct
		if (!romToLaunch.empty()){
			LOG_DEBUG("Launching rom %s", romToLaunch.c_str());
			gameMenu->clearOverlay();
			if (launchGame(romToLaunch, deleteTmpDir)){
				gameMenu->setEmuStatus(EMU_STARTED);

				//Setting the romname to the Faqs downloader menu
				if (!game->gameTitle.empty()){
					gameMenu->configMenus->setGameLoaded(game->gameTitle);
				} else {
					gameMenu->configMenus->setGameLoaded(romToLaunch);
				}
			}
			return 0;
		}
	}

	//Mostramos la primera imagen del art del juego
	const int sdlBtnModif = gameMenu->joystick->inputs.mapperHotkeys.getSdlBtn(0, HK_MODIFIER);
	if ((sdlBtnModif == -1 || !gameMenu->joystick->inputs.getSdlBtn(0, sdlBtnModif)) && gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_Y) && gameMenu->someImageLoaded()) {
		gameMenu->findFirstImage();
		gameMenu->setEmuStatus(EMU_MENU_IMAGE_VIEWER);
	}

	//Mostramos el filtro de juegos
	if (gameMenu->joystick->inputs.getBtnTap(0, JOY_BUTTON_L3)){
		gameMenu->setEmuStatus(EMU_MENU_FILTER);
	}

	return 1;
}

int processInputs(GameMenu*& gameMenu, ListMenu &listMenu, bool generalConfig){
	int res = 1;

	if (gameMenu->configMenus->getStatus() == POLLING_INPUTS){
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				
				case SDL_QUIT:
					gameMenu->running = false;
					break;
				case SDL_JOYBUTTONDOWN:
					//LOG_INFO("Boton detectado: ID %d", (int)event.jbutton.button);
					gameMenu->configMenus->updateButton(event, KEY_JOY_BTN); //event.jbutton.button
					if (gameMenu->configMenus->isFrontendKeysMenu()){
						gameMenu->joystick->setInfoButtons();
					}
					break;
				case SDL_JOYHATMOTION:
					//LOG_INFO("hat detectado: ID %d", (int)event.jhat.value);
					if (event.jhat.value != 0){ //Solo en el momento del joydown
						gameMenu->configMenus->updateButton(event, KEY_JOY_HAT); //event.jhat.value
                    }
                    break;
				case SDL_JOYAXISMOTION:
					//LOG_INFO("axis detectado: value %d axis: %d", (int)event.jaxis.value, (int)event.jaxis.axis);
					gameMenu->configMenus->updateAxis(event); //event.jaxis.value, event.jaxis.axis
					break;
				default:
					break;
			}
		}
		
	} else {
		gameMenu->joystick->pollKeys(gameMenu->overlay);
		

		if (gameMenu->isOnscreenKeybEnabled()){
			//Se procesan las acciones del teclado que se muestra en un overlay. Solo MSX y SPECTRUM
			procesarOverlayTeclado();
		} else if (generalConfig){
			//Se procesan las acciones del menu de configuracion general
			if (procesarGeneralConfig() == 1)
				return 1;
		} else if (gameMenu->getEmuStatus() == EMU_MENU_FILTER){
			Menu *menuFilter = gameMenu->configMenus->menuGameFilter;
			
			if (gameMenu->joystick->inputs.getBtnTap(0, JOY_BUTTON_B) || gameMenu->joystick->inputs.getBtnTap(0, JOY_BUTTON_L3)) {
				//Boton L3 o B para cancelar
				gameMenu->setEmuStatus(gameMenu->getLastStatus());
			} else if (gameMenu->joystick->inputs.getBtnTap(0, JOY_BUTTON_A)) {
				//Boton A para cambiar las opciones de booleanos
				if (menuFilter->opciones[menuFilter->seleccionado]->tipo == OPC_BOOLEANA){
					OpcionBool* b = (OpcionBool*)menuFilter->opciones[menuFilter->seleccionado];
					*b->valor = !*b->valor;
				} 
				listMenu.checkFilter();
			} else if (gameMenu->joystick->inputs.getBtnTap(0, JOY_BUTTON_X)) {
				//Boton X para resetear el filtro
				if (menuFilter->opciones[menuFilter->seleccionado]->tipo == OPC_LISTA_REF){
					OpcionListaRef* l = (OpcionListaRef*)menuFilter->opciones[menuFilter->seleccionado];
					*l->indice = -1;
				}
				listMenu.checkFilter();
			} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_RIGHT)) {
				//Boton Derecho para cambiar las opciones de listas
				if (menuFilter->opciones[menuFilter->seleccionado]->tipo == OPC_LISTA_REF){
					OpcionListaRef* l = (OpcionListaRef*)menuFilter->opciones[menuFilter->seleccionado];
					auto items = *l->items;
					if (items.empty() || *l->indice == (int)items.size() - 1){
						*l->indice = -1;
						listMenu.checkFilter();
					} else if (items.size() > 0){
						*l->indice = (*l->indice + 1) % (int)items.size();
						listMenu.checkFilter();
					}
				}
			} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_LEFT)) {
				//Boton Izquierdo para cambiar las opciones de listas
				if (menuFilter->opciones[menuFilter->seleccionado]->tipo == OPC_LISTA_REF){
					OpcionListaRef* l = (OpcionListaRef*)menuFilter->opciones[menuFilter->seleccionado];
					auto items = *l->items;
					if (items.empty() || *l->indice == 0){
						*l->indice = -1;
						listMenu.checkFilter();
					} else if (items.size() > 0){
						if (*l->indice == -1){
							*l->indice = items.size() - 1;
						} else {
							*l->indice = *l->indice - 1;
						}
						listMenu.checkFilter();
					}
				}
			} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_DOWN) && !menuFilter->opciones.empty()) {
				menuFilter->seleccionado = (menuFilter->seleccionado + 1) % (int)menuFilter->opciones.size();
			} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_UP) && !menuFilter->opciones.empty()) {
				menuFilter->seleccionado = (menuFilter->seleccionado + (int)menuFilter->opciones.size() - 1)
										  % (int)menuFilter->opciones.size();
			} 
		} else if (gameMenu->getEmuStatus() == EMU_MENU_IMAGE_VIEWER){
			if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_Y) || gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_B)) {
				gameMenu->setEmuStatus(gameMenu->getLastStatus());
				gameMenu->selectedFsImage = 0;
				return 0;
			} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_RIGHT)) {
				gameMenu->nextImageLoaded();
			} else if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_LEFT)) {
				gameMenu->prevImageLoaded();
			}
			return 1;
		} else {
			//Acciones sobre listMenu
			if (procesarAccionesMenu(listMenu) == 0)
				return 0;
		}
	
		const bool showEmpty = CfgLoader::configMain[cfg::showEmptyEmulators].valueBool;
		CfgLoader *cfg = gameMenu->getCfgLoader();
		const int firstPos = cfg->emuCfgPos;

		if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_R) && gameMenu->getEmuStatus() == EMU_MENU){
			listMenu.listZipped.clear();
			listMenu.listDir.clear();
			listMenu.resizeMarginTop(0, gameMenu->overlay->h);

			do{
				cfg->getNextCfgEmu();
				gameMenu->loadEmuCfg(listMenu);
			} while (listMenu.getNumGames() == 0 && cfg->emuCfgPos != firstPos && !showEmpty && !cfg->getCfgEmu()->generalConfig);

			//Set the keyboard layout
			gameMenu->keyb->setKeyboardLayout(cfg->getCfgEmu()->keyboard_type, gameMenu->overlay->w, gameMenu->overlay->h);
		}

		if (gameMenu->joystick->inputs.getAnyTap(0, JOY_BUTTON_L) && gameMenu->getEmuStatus() == EMU_MENU){
			listMenu.listZipped.clear();
			listMenu.listDir.clear();
			listMenu.resizeMarginTop(0, gameMenu->overlay->h);
			
			do {
				cfg->getPrevCfgEmu();
				gameMenu->loadEmuCfg(listMenu);
			} while (listMenu.getNumGames() == 0 && cfg->emuCfgPos != firstPos && !showEmpty && !cfg->getCfgEmu()->generalConfig);

			//Set the keyboard layout
			gameMenu->keyb->setKeyboardLayout(cfg->getCfgEmu()->keyboard_type, gameMenu->overlay->w, gameMenu->overlay->h);
		}

		listMenu.updateAssets = gameMenu->joystick->inputs.getAnyReleased(0, JOY_BUTTON_UP) ||
				gameMenu->joystick->inputs.getAnyReleased(0, JOY_BUTTON_DOWN) ||
				gameMenu->joystick->inputs.getAnyReleased(0, JOY_BUTTON_LEFT) ||
				gameMenu->joystick->inputs.getAnyReleased(0, JOY_BUTTON_RIGHT)||
				gameMenu->joystick->inputs.getAnyReleased(0, JOY_BUTTON_L)    ||
				gameMenu->joystick->inputs.getAnyReleased(0, JOY_BUTTON_R)    ||
				gameMenu->gameTicks.ticks == 0;

		if (HK_VIEW_MENU == gameMenu->joystick->hotkeys->procesarHotkeys(&gameMenu->joystick->inputs)){
			if (gameMenu->getLastStatus() == EMU_STARTED){
				gameMenu->clearOverlay();
			}
			gameMenu->setEmuStatus(gameMenu->getLastStatus());
			if (gameMenu->bg_screenshot){
				SDL_FreeSurface(gameMenu->bg_screenshot);
				gameMenu->bg_screenshot = NULL;
			}
			gameMenu->bg_screenshot = gameMenu->clonarPantalla(gameMenu->gameScreen, 180);
			return 0;
		}

		gameMenu->running = !gameMenu->joystick->evento.quit && gameMenu->running;
	}

	return res;
}

/**
 * Shows the background and all the menu elements
 */
void updateMenuScreen(TileMap &tileMap, GameMenu*& gameMenu, ListMenu &listMenu){
	static uint32_t lastTime = SDL_GetTicks();
	cfg::t_cfg_props* cfg = gameMenu->getCfgLoader()->configMain;
	ConfigEmu *emu = gameMenu->getCfgLoader()->getCfgEmu();

	ANIM_BACKGROUNDS bgType = static_cast<ANIM_BACKGROUNDS>(gameMenu->getCfgLoader()->configMain[cfg::animBG].valueInt);

	if (processInputs(gameMenu, listMenu, emu->generalConfig) == 1 && gameMenu->getEmuStatus() != EMU_STARTED){
		//Draw the background
		if (listMenu.animateBkg && bgType >= BG_HLSL && bgType < BG_NONE){
			HLSLBackground_setActive(bgType - BG_HLSL + 1);
			gameMenu->clearOverlay();
		} else if (listMenu.animateBkg && bgType == BG_TILES){
			tileMap.draw(gameMenu->overlay);
			if (SDL_GetTicks() - lastTime > bkgFrameTimeTick && (lastTime = SDL_GetTicks()) > 0){
				tileMap.incSpeed();
			}
		} else if (bgType == BG_IMAGE && gameMenu->bg_image.hasImage()){
			gameMenu->bg_image.printImage(gameMenu->overlay);
		} else if ((cfg[cfg::enableAchievements].valueBool && cfg[cfg::hardcoreRA].valueBool) || !gameMenu->bg_screenshot){
			gameMenu->fillOverlay(clBackground);
		} else if (gameMenu->bg_screenshot){
			SDL_BlitSurface(gameMenu->bg_screenshot, NULL, gameMenu->overlay, NULL);
		} else {
			gameMenu->fillOverlay(clBackground);
		}

		//Draw the menu
		gameMenu->refreshScreen(listMenu);
	}
}

/**
* Shows a menu to edit the options while being ingame
*/
void updateMenuOverlay(GameMenu*& gameMenu, ListMenu &listMenu){
	processInputs(gameMenu, listMenu, true);
	if (gameMenu->getEmuStatus() == EMU_MENU_OVERLAY){
		cfg::t_cfg_props* cfg = gameMenu->getCfgLoader()->configMain;
		//Si el modo hardcore esta activado, o no hay captura de pantalla, mostramos el menu en negro
		if ((cfg[cfg::enableAchievements].valueBool && cfg[cfg::hardcoreRA].valueBool) || !gameMenu->bg_screenshot){
			gameMenu->fillOverlay(clBackground);
		} else if (gameMenu->bg_screenshot){
			SDL_BlitSurface(gameMenu->bg_screenshot, NULL, gameMenu->overlay, NULL);
		} 
		gameMenu->configMenus->draw(gameMenu->overlay);
	}
}	

