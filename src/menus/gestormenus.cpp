#include <algorithm> // Imprescindible para std::sort
#include <math.h>

#include <menus/gestormenus.h>
#include <const/constant.h>
#include <const/menuconst.h>
#include <gfx/gfx_utils.h>
#include <gfx/SDL_gfxPrimitives.h>
#include <font/fonts.h>
#include <io/joystick.h>

SDL_Surface* GestorMenus::imgText;

GestorMenus::GestorMenus(int screenw, int screenh){
	menuRaiz = NULL;
	menuActual = NULL;
	setObjectType(GUIOPTIONS);
	iniPos = 0;
    endPos = 0;
    curPos = 0;
    listSize = 0;
    maxLines = 0;
    marginX = (int)floor((double)(screenw / 100));
    marginY = (int) (screenh / SCREENHDIV * 1.5);
    lastSel = -1;
    pixelShift = 0;
    keyUp = false;
	this->setLayout(0, screenw, screenh);
	status = NORMAL;
	options_changed_flag = false;

	const int box2dW = screenw / 2 - 2 * marginX;
	const int box2dH = box2dW;
    
	imageMenu.setX(screenw - box2dW - marginX);
    imageMenu.setY(getY() + 3);
    imageMenu.setW(box2dW);
	imageMenu.setH(box2dH);
	askNumOptions = 0;
}

GestorMenus::~GestorMenus() {
    for(std::size_t i = 0; i < todosLosMenus.size(); i++) 
		delete todosLosMenus[i];
}

std::string guardarJoysticks(Joystick* joy){
	LOG_DEBUG("Guardando valores del joystick");
	return "Fichero guardado en: " + joy->saveButtonsRetro();
}

std::string guardarCoreConfig(CfgLoader *refConfig){
	LOG_DEBUG("Guardando valores del core actual");
	return refConfig->saveCoreParams();
}

std::string guardarMainConfig(CfgLoader *refConfig){
	LOG_DEBUG("Guardando valores principales de configuracion");
	return refConfig->saveMainParams();
}

void GestorMenus::setLayout(int layout, int screenw, int screenh){
	this->marginY = (int) (screenh / SCREENHDIV * 1.5);
    clearSelectedText();
  
    this->setX(marginX);
    this->setY(marginY);
    this->setW(screenw - marginX);
    this->setH(screenh - marginY);
    this->centerText = false;
    this->layout = layout;
}

// Inicializa la estructura de menús
void GestorMenus::inicializar(CfgLoader *refConfig, Joystick *joystick) {
    // 1. Crear contenedores de menús
    menuRaiz = new Menu("Opciones");
    Menu* menuVideo = new Menu("Vídeo", menuRaiz);
	Menu* menuEmulation = new Menu("Emulación", menuRaiz);
	Menu* menuEntrada = new Menu("Entrada", menuRaiz);
	menuCoreOptions = new Menu("Opciones del core", menuRaiz);
	menuSavestates = new Menu("Partidas guardadas", menuRaiz);

	//Este menu no cuelga de ningun lado, pero ponemos partidas guardadas como padre
	menuAskSavestates = new Menu("Gestionar Partidas guardadas", menuSavestates);
        
    todosLosMenus.push_back(menuRaiz);
    todosLosMenus.push_back(menuVideo);
	todosLosMenus.push_back(menuEmulation);
	todosLosMenus.push_back(menuEntrada);
	todosLosMenus.push_back(menuCoreOptions);
	todosLosMenus.push_back(menuSavestates);
	todosLosMenus.push_back(menuAskSavestates);

	//Poblar menu emulacion
	//Escalado de video
    std::vector<std::string> syncvals;
	for (int i=0; i < TOTAL_VIDEO_SYNC; i++){
		syncvals.push_back(syncOptionsStrings[i]);
	}
	menuEmulation->opciones.push_back(new OpcionLista("Sincronización", syncvals, &refConfig->configMain[cfg::syncMode].getIntRef()));
	menuEmulation->opciones.push_back(new OpcionBool("Mostrar fps", &refConfig->configMain[cfg::showFps].getBoolRef()));

    //Poblar Menú Video
	//Relacion de aspecto
	std::vector<std::string> aspectRates;
	for (int i=0; i < TOTAL_VIDEO_RATIO; i++){
		aspectRates.push_back(aspectRatioStrings[i]);
	}
	menuVideo->opciones.push_back(new OpcionLista("Relación de aspecto", aspectRates, &refConfig->configMain[cfg::aspectRatio].getIntRef()));

	//Escalado de video
    std::vector<std::string> filtros;
	for (int i=0; i < TOTAL_VIDEO_SCALE; i++){
		filtros.push_back(videoScaleStrings[i]);
	}
	menuVideo->opciones.push_back(new OpcionLista("Escalado", filtros, &refConfig->configMain[cfg::scaleMode].getIntRef()));
	menuVideo->opciones.push_back(new OpcionBool("Forzar pantalla completa", &refConfig->configMain[cfg::forceFS].getBoolRef()));

	Menu* menuAsignaciones = new Menu("Asignaciones de Retropad", menuEntrada);
	Menu* menuHotkeys = new Menu("Teclas rápidas", menuEntrada);
	todosLosMenus.push_back(menuAsignaciones);
	todosLosMenus.push_back(menuHotkeys);

	menuEntrada->opciones.push_back(new OpcionSubMenu("Asignaciones de Retropad", menuAsignaciones));
	menuEntrada->opciones.push_back(new OpcionSubMenu("Teclas rápidas", menuHotkeys));
	menuEntrada->opciones.push_back(new OpcionExec<Joystick>("Guardar asignaciones", guardarJoysticks, joystick));


	for (int controlId = 0; controlId < MAX_PLAYERS; controlId++){
		std::string controlStr = "Controles del puerto " + Constant::TipoToStr(controlId + 1) + " " +
			joystick->buttonsMapperLibretro[controlId].joyName;
		Menu* menuControlesPuerto = new Menu(controlStr , menuAsignaciones);
		addControlerOptions(menuControlesPuerto, controlId, joystick, refConfig);
		addControlerButtons(menuControlesPuerto, controlId);
		menuAsignaciones->opciones.push_back(new OpcionSubMenu(controlStr, menuControlesPuerto));
		todosLosMenus.push_back(menuControlesPuerto);
	}
	
	//Poblar menu hotkeys
	poblarMenuHotkeys(menuHotkeys, joystick);

	//Poblar menu ask
	std::vector<std::string> askOptions;
	for (int i=0; i < MAX_ASK; i++){
		askOptions.push_back(ACTION_ASK_STR[i]);
	}
	menuAskSavestates->opciones.push_back(new OpcionLista("Seleccionar acción", askOptions, &askNumOptions));

    // 3. Poblar Menú Principal
    menuRaiz->opciones.push_back(new OpcionSubMenu("Configuración vídeo", menuVideo));
	menuRaiz->opciones.push_back(new OpcionSubMenu("Configuración emulación", menuEmulation));
	menuRaiz->opciones.push_back(new OpcionSubMenu("Entrada", menuEntrada));
	menuRaiz->opciones.push_back(new OpcionSubMenu("Opciones del core", menuCoreOptions));
	menuRaiz->opciones.push_back(new OpcionSubMenu("Partidas guardadas", menuSavestates));
	menuRaiz->opciones.push_back(new OpcionExec<CfgLoader>("Guardar opciones", guardarMainConfig, refConfig));

    // Establecer estado inicial
    menuActual = menuRaiz;
	resetIndexPos();
}

void GestorMenus::poblarMenuHotkeys(Menu* menuHotkeys, Joystick *joystick){
	TipoKey type = KEY_JOY_BTN;

	menuHotkeys->opciones.push_back(new OpcionKey("Tecla para activar teclas rápidas", &joystick->hotkeys.g_modifierButton, type, TipoKeyStr[type]));

	for (int i=0; i < joystick->hotkeys.g_hotkeys.size(); i++){
		HotkeyConfig *hkCfg = &joystick->hotkeys.g_hotkeys[i];
		if (hkCfg->action < HK_MAX){
			if (hkCfg->triggerButton == JOY_BUTTON_DOWN || hkCfg->triggerButton == JOY_BUTTON_UP 
				|| hkCfg->triggerButton == JOY_BUTTON_LEFT || hkCfg->triggerButton == JOY_BUTTON_RIGHT){
					type = KEY_JOY_HAT;
			} else {
				type = KEY_JOY_BTN;
			}
			menuHotkeys->opciones.push_back(new OpcionKey(HOTKEYS_STR[hkCfg->action], &hkCfg->triggerButton, type, TipoKeyStr[type]));
		}
	}
}

void GestorMenus::addControlerOptions(Menu*& menu, int controlId, Joystick *joystick, CfgLoader *refConfig){
	menu->opciones.clear();
	if (controlId < MAX_PLAYERS){
		auto& controllerPad = refConfig->g_ports[controlId];
		std::vector<std::string> gamepads;

		for (int i=0; i < controllerPad.available_types.size(); i++){
			if (i == 0 && controllerPad.current_device_id < 0){
				controllerPad.current_device_id = controllerPad.available_types.at(i).first;
				controllerPad.current_desc = controllerPad.available_types.at(i).second;
			}
			gamepads.push_back(controllerPad.available_types.at(i).second);
		}
		if (controllerPad.available_types.size() > 0){
			menuCoreOptions->opciones.push_back(new OpcionLista(controllerPad.current_desc, gamepads, &controllerPad.current_device_id));
		}
	}
	menu->opciones.push_back(new OpcionBool("Eje analógico como pad", &joystick->buttonsMapperLibretro[controlId].axisAsPad));
}

void GestorMenus::poblarCoreOptions(CfgLoader *refConfig){
    auto& params = refConfig->startupLibretroParams;
    
    // Estructura temporal para ordenar
    struct TempElem {
        std::string key;
        std::string desc;
    };
    std::vector<TempElem> sorter;

    // 1. Llenamos el vector con la clave y la descripción
    for (auto it = params.begin(); it != params.end(); ++it) {
        TempElem e = { it->first, it->second->description };
        sorter.push_back(e);
    }

    // 2. Ordenamos por descripción usando una lambda o función estática
    std::sort(sorter.begin(), sorter.end(), [](const TempElem& a, const TempElem& b) {
        return Constant::compareNoCase(a.desc, b.desc);
    });

	menuCoreOptions->opciones.push_back(new OpcionExec<CfgLoader>("Guardar opciones", guardarCoreConfig, refConfig));

	menuCoreOptions->opciones.push_back(new OpcionTxtAndValue("Versión del core", refConfig->configMain[cfg::libretro_core].valueStr + " " + refConfig->configMain[cfg::libretro_core_version].valueStr));
	menuCoreOptions->opciones.push_back(new OpcionTxtAndValue("Extensiones admitidas", refConfig->configMain[cfg::libretro_core_extensions].valueStr));

    // 3. Ahora recorremos el vector ordenado y buscamos en el mapa original por KEY
    for (auto it = sorter.begin(); it != sorter.end(); ++it) {
        auto elem = params.find(it->key); // Ahora sí buscamos por la clave correcta
        
        if (elem != params.end()) { // Validación de seguridad fundamental
            cfg::t_emu_props* props = elem->second.get();
            if (props) {
                LOG_INFO("Key: %s, Selected: %d", elem->first.c_str(), props->selected);
                menuCoreOptions->opciones.push_back(new OpcionLista(props->description, props->values, &props->selected));
            }
        }
    }
}

/**
*
*/
void GestorMenus::poblarPartidasGuardadas(CfgLoader *refConfig, std::string rompath){
	dirutil dir;
	const std::string statesDir = refConfig->configMain[cfg::libretro_state].valueStr + Constant::getFileSep() +
		refConfig->configMain[cfg::libretro_core].valueStr;
	const std::string keyToFind = STATE_EXT;
	const std::string filterName = dir.getFileNameNoExt(rompath) + keyToFind;
	std::size_t pos = 0;
	std::string posSlot = "0";
	int found = -1;
	vector<unique_ptr<FileProps>> files;

	dir.listarFilesSuperFast(statesDir.c_str(), files, "", filterName, true, true);
	menuSavestates->opciones.clear();

	// 1. Inicializar con objetos vacíos (opcional, dependiendo de tu lógica de UI)
	for (int i = 0; i < MAX_SAVESTATES; ++i) {
		menuSavestates->opciones.push_back(new OpcionSavestate("- Vacío -"));
	}

	for (std::size_t i = 0; i < files.size(); ++i) {
		// Validaciones iniciales
		if (!files[i] || dir.getExtension(files[i]->filename) == STATE_IMG_EXT) continue;

		std::size_t pos = files[i]->filename.find(keyToFind);
		if (pos == std::string::npos) continue;

		// Extraer índice de la ranura
		std::string posSlot = files[i]->filename.substr(pos + keyToFind.length());
		int iPosSlot = Constant::strToTipo<int>(posSlot);

		if (iPosSlot >= 0 && iPosSlot < (int)menuSavestates->opciones.size()) {
			// Usar un puntero temporal para legibilidad y evitar múltiples casteos
			OpcionSavestate* opt = static_cast<OpcionSavestate*>(menuSavestates->opciones[iPosSlot]);
			// FileProps con copia segura
			opt->file = *files[i]; 
			// Para poder modificar el status si se pulsa este elemento y poder mostrar la emergente
			opt->status = &this->status; 
			// Titulo del elemento
			opt->titulo = "Ranura " + (posSlot.empty() ? "0" : posSlot);
		}
	}
}

void GestorMenus::addControlerButtons(Menu*& menu, int controlId){
	int num_port_buttons = sizeof(configurablePortButtons) / sizeof(configurablePortButtons[0]);
	int num_port_hats = sizeof(configurablePortHats) / sizeof(configurablePortHats[0]);

	static int notfound = -1;
	std::string text;

	const int nBtns = Joystick::buttonsMapperLibretro[controlId].nButtons;
	const int nHats = Joystick::buttonsMapperLibretro[controlId].nHats;
	const int nAxis = Joystick::buttonsMapperLibretro[controlId].nAxis;

	int retroAxisValue = 0;
	int axisPos = 0;

	t_joy_retro_inputs *input = &Joystick::buttonsMapperLibretro[controlId];
	//Adding the axis or pad elements
	for (int retroBtnIdx=0; retroBtnIdx < num_port_hats; retroBtnIdx++){
		const std::string text = configurablePortHatsStr[retroBtnIdx];
		const int retroBtnValue = configurablePortHats[retroBtnIdx];

		if (input->axisAsPad){
			int8_t axisIdx = input->findButtonIdx(retroBtnValue, input->axis);
			menu->opciones.push_back(new OpcionKey(text, Joystick::buttonsMapperLibretro, controlId, axisIdx, retroBtnValue, KEY_JOY_AXIS, TipoKeyStr[KEY_JOY_AXIS]));
		} else {
			int8_t hatIdx = input->findButtonIdx(retroBtnValue, input->hats);
			menu->opciones.push_back(new OpcionKey(text, Joystick::buttonsMapperLibretro, controlId, hatIdx, retroBtnValue, KEY_JOY_HAT, TipoKeyStr[KEY_JOY_HAT]));
		}
	}

	//Adding the buttons elements
	for (int sdlBtnIdx=0; sdlBtnIdx < num_port_buttons; sdlBtnIdx++){
		std::string text = configurablePortButtonsStr[sdlBtnIdx];
		const int retroBtnValue = configurablePortButtons[sdlBtnIdx];
		
		int8_t btnIdx = input->findButtonIdx(retroBtnValue, input->buttons);
		int8_t axisIdx = input->findButtonIdx(retroBtnValue, input->axis);

		if (btnIdx > -1 || axisIdx == -1){
			menu->opciones.push_back(new OpcionKey(text, Joystick::buttonsMapperLibretro, controlId, btnIdx, retroBtnValue, KEY_JOY_BTN, TipoKeyStr[KEY_JOY_BTN]));	
		} else if (axisIdx > -1){
			menu->opciones.push_back(new OpcionKey(text, Joystick::buttonsMapperLibretro, controlId, axisIdx, retroBtnValue, KEY_JOY_AXIS, TipoKeyStr[KEY_JOY_AXIS]));
		} 		
	}
}

int GestorMenus::findAxisPos(int retroDirection){
	int num_port_hats = sizeof(configurablePortHats) / sizeof(configurablePortHats[0]);
	for (int i=0; i < num_port_hats; i++){
		if (configurablePortHats[i] == retroDirection){
			return i;
		}
	}
	return -1;
}

// Lógica para cambiar valores (Izquierda / Derecha)
void GestorMenus::cambiarValor(int dir) {
	if (status == POLLING_INPUTS || menuActual->opciones.size() == 0) return;

	if (status == ASK_SAVESTATES){
		Opcion* opt = menuAskSavestates->opciones[0];
		if (opt->tipo == OPC_LISTA) {
			OpcionLista* l = (OpcionLista*)opt;
			int num = (int)l->items.size();
			if (num > 0){
				*(l->indice) = (*(l->indice) + dir + num) % num;
			}
		}
		return;
	}

    Opcion* opt = menuActual->opciones[menuActual->seleccionado];
    if (opt->tipo == OPC_LISTA) {
        OpcionLista* l = (OpcionLista*)opt;
        int num = (int)l->items.size();
		if (num > 0){
			*(l->indice) = (*(l->indice) + dir + num) % num;
		}
    } else if (opt->tipo == OPC_BOOLEANA) {
        OpcionBool* b = (OpcionBool*)opt;
		LOG_DEBUG("cambiando de %s\n", *(b->valor) ? "S" : "N");
        *(b->valor) = !(*(b->valor));
		LOG_DEBUG("a %s\n",  *(b->valor) ? "S" : "N");
    } else if (opt->tipo == OPC_INT) {
		OpcionInt* i = (OpcionInt*)opt;
	} 
}

// Lógica para confirmar (Botón A)
std::string GestorMenus::confirmar(t_option_action *result) {
	if (status == POLLING_INPUTS || menuActual->opciones.size() == 0) return "";

	if (status == ASK_SAVESTATES){
		Opcion* e = menuAskSavestates->opciones[0];
		if (e->tipo == OPC_LISTA) {
			OpcionLista* l = (OpcionLista*)e;
			Opcion* opcionDelPadre = menuAskSavestates->padre->opciones[menuAskSavestates->padre->seleccionado];
			if (opcionDelPadre->tipo == OPC_SAVESTATE){
				OpcionSavestate *optSaves = static_cast<OpcionSavestate*>(opcionDelPadre);
				result->option = opcionDelPadre->tipo;
				result->action = *l->indice;
				result->indexSelected = menuAskSavestates->padre->seleccionado;

				std::string filepath = optSaves->file.dir + Constant::getFileSep() + optSaves->file.filename;
				if (!filepath.empty()) {
					result->elem = (void*)_strdup(filepath.c_str());
				} else {
					result->elem = NULL;
				}
			}
		}
		return e->ejecutar();
	}

    Opcion* opt = menuActual->opciones[menuActual->seleccionado];
    if (opt->tipo == OPC_SUBMENU) {
        menuActual = ((OpcionSubMenu*)opt)->destino;
		resetIndexPos();
    } else if (opt->tipo == OPC_BOOLEANA) {
        cambiarValor(1);
	} else if (opt->tipo == OPC_KEY) {
		OpcionKey* k = (OpcionKey*)opt;
		k->changeAsked = true;
		k->lastTimeAsked = SDL_GetTicks();
		status = POLLING_INPUTS;
    } else if (opt->tipo == OPC_EXEC) {
		Opcion* e = (Opcion*)opt;
		return e->ejecutar();
	} else if (opt->tipo == OPC_SAVESTATE) {
		Opcion* e = (Opcion*)opt;
		return e->ejecutar();
	}

	return std::string("");
}

// Lógica para volver (Botón B)
void GestorMenus::volver() {
	if (status == POLLING_INPUTS) return;

	if (status == ASK_SAVESTATES){
		LOG_DEBUG("Volviendo al menu de savestates");
		menuActual = menuAskSavestates->padre;
		status = NORMAL;
		resetIndexPos();
		return;
	}

    if (menuActual->padre != NULL) {
        menuActual = menuActual->padre;
		resetIndexPos();
    }
}

void GestorMenus::resetKeyElement(int sdlbtn, TipoKey tipoKey){
	//Buscamos en todos los elementos de menu y si hay alguna opcion con el mismo indice, lo ponemos a -1
	std::vector<Opcion*> optButtons = menuActual->opciones;
	for (int i=0; i < optButtons.size(); i++){
		if (optButtons[i]->tipo == OPC_KEY){
			OpcionKey* keyToReset = static_cast<OpcionKey*>(optButtons[i]);
			if (keyToReset->idx == sdlbtn && tipoKey == keyToReset->tipoKey){
				keyToReset->idx = -1;
			}
		}
	}
}

/**
*
*/
void GestorMenus::updateAxis(int sdlAxisValue, int sdlAxis){
	if (status != POLLING_INPUTS) return;
	Opcion* opt = menuActual->opciones[menuActual->seleccionado];

	if (opt->tipo == OPC_KEY) {
		OpcionKey* k = static_cast<OpcionKey*>(opt);
		if (k && k->joyInputs) {
			if (abs(sdlAxisValue) > DEADZONE) {
				// 0 si es negativo (Izquierda/Arriba), 1 si es positivo (Derecha/Abajo)
				int isPositive = (sdlAxisValue > 0);
				int buttonIdx = (sdlAxis * 2) + isPositive;
				LOG_DEBUG("Eje: %d, Valor: %d -> Boton Virtual: %d", sdlAxis, sdlAxisValue, buttonIdx);
				k->tipoKey = KEY_JOY_AXIS;
				k->description = TipoKeyStr[k->tipoKey];
				resetKeyElement(buttonIdx, k->tipoKey);
				k->joyInputs->setAxis(buttonIdx, k->btn);
				k->idx = buttonIdx;
				k->changeAsked = false;
				k->lastTimeAsked = 0;
				status = NORMAL;
				//La posicion de la opcion 0 es el elemento que anyadimos en addControlerOptions
				//en el orden de las inserciones en el vector.
				if (menuActual->opciones.size() > 0 && menuActual->opciones[0]->tipo == OPC_BOOLEANA) {	
					//Ponemos a true la opcion "Eje analógico como pad"
					OpcionBool* b = (OpcionBool*)menuActual->opciones[0];
					*(b->valor) = true;
				}
			} else {
				// CENTRO: Opcionalmente manejar el reposo aquí si es necesario
			}
		}
	}
}

/**
*
*/
void GestorMenus::updateButton(int sdlbtn, TipoKey tipoKey){
	if (status != POLLING_INPUTS) return;
	Opcion* opt = menuActual->opciones[menuActual->seleccionado];

	if (opt->tipo == OPC_KEY) {
		OpcionKey* k = static_cast<OpcionKey*>(opt);
		if (k && k->joyInputs) {
			if (k->tipoKey == KEY_JOY_BTN){
				//Actualizamos el elemento anterior a -1, tanto en el array
				k->joyInputs->setButton(k->idx, -1);
				//como en la propia tecla de la opcion del menu
				if (k->idx != sdlbtn)
					resetKeyElement(sdlbtn, k->tipoKey);
				//Asignamos el boton del joystick pulsado con su funcion de libretro
				k->joyInputs->setButton(sdlbtn, k->btn);
				//Actualizamos el indice seleccionado para esta tecla
				k->idx = sdlbtn;
			} else if (k->tipoKey == KEY_JOY_HAT || k->tipoKey == KEY_JOY_AXIS){
				// Extraemos la dirección activa del Hat (limpiamos otros bits si fuera necesario)
				Uint8 sdlHatDir = (Uint8)(sdlbtn & (SDL_HAT_UP | SDL_HAT_DOWN | SDL_HAT_LEFT | SDL_HAT_RIGHT));
				//En cualquier caso, indicamos que este boton se comporta como un hat
				k->tipoKey = KEY_JOY_HAT;
				k->description = TipoKeyStr[k->tipoKey];
				//Actualizamos el elemento anterior a -1, tanto en el array
				k->joyInputs->setHat(k->idx, -1);
				//como en la propia tecla de la opcion del menu
				if (k->idx != sdlHatDir)
					resetKeyElement(sdlHatDir, k->tipoKey);
				//Asignamos el boton del joystick pulsado con su funcion de libretro
				k->joyInputs->setHat(sdlHatDir, k->btn);
				//Actualizamos el indice seleccionado para esta tecla
				k->idx = sdlHatDir;
			}
			//Reseteamos el estado
			k->changeAsked = false;
			k->lastTimeAsked = 0;
			status = NORMAL;
		} else if (k && k->intRef) {
			int btnToSend = sdlbtn;
			k->description = TipoKeyStr[tipoKey];
			if (tipoKey == KEY_JOY_HAT && (sdlbtn == SDL_HAT_DOWN || sdlbtn == SDL_HAT_UP || sdlbtn == SDL_HAT_LEFT || sdlbtn == SDL_HAT_RIGHT)){
				k->tipoKey == KEY_JOY_HAT;
				switch (sdlbtn){
					case SDL_HAT_DOWN:
						btnToSend = JOY_BUTTON_DOWN;
						break;
					case SDL_HAT_UP:
						btnToSend = JOY_BUTTON_UP;
						break;
					case SDL_HAT_LEFT:
						btnToSend = JOY_BUTTON_LEFT;
						break;
					case SDL_HAT_RIGHT:
						btnToSend = JOY_BUTTON_RIGHT;
						break;
				}
			} else {
				k->tipoKey == KEY_JOY_BTN;
			}

			k->idx = btnToSend;
			*k->intRef = btnToSend;
			k->changeAsked = false;
			k->lastTimeAsked = 0;
			status = NORMAL;
		}
	}
}

Menu* GestorMenus::obtenerMenuActual() {
	return menuActual; 
}

void GestorMenus::draw(SDL_Surface *video_page){
	static const int bkg = SDL_MapRGB(video_page->format, bkgMenu.r, bkgMenu.g, bkgMenu.b);
	static const int iwhite = SDL_MapRGB(video_page->format, white.r, white.g, white.b);
	static const int iblack = SDL_MapRGB(video_page->format, black.r, black.g, black.b);
	static const int iswitchenabled = SDL_MapRGB(video_page->format, 200, 200, 200);
	static const int iswitchdisabled = SDL_MapRGB(video_page->format, 77, 77, 77);

	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	int face_h = TTF_FontLineSkip(fontMenu);

	Constant::drawTextCentTransparent(video_page, fontMenu, this->menuActual->titulo.c_str(), 0, face_h < marginY ? (marginY - face_h) / 2 : 0 , 
				true, false, textColor, 0);
	fastline(video_page, marginX, marginY - 1, video_page->w - marginX, marginY - 1, textColor);

    //To scroll one letter in one second. We use the face_h because the width of 
    //a letter is not fixed.
    const float pixelsScrollFps = std::max(ceil(face_h / (float)textFps), 1.0f);

    for (int i=this->iniPos; i < this->endPos; i++){
        auto option = this->menuActual->opciones.at(i);
       
		std::string line;
		std::string value;

		if (option->tipo == OPC_SAVESTATE){
			drawSavestateWithImage(i, (OpcionSavestate *) option, video_page);
			continue;
		} else if (option->tipo == OPC_BOOLEANA){
			line = option->titulo;// + " " + std::string(*((OpcionBool *)option)->valor ? "Y" : "N");
		} else if (option->tipo == OPC_LISTA){
			int indice = *((OpcionLista *)option)->indice;
			line = option->titulo;
			value = "< " + ((OpcionLista *)option)->items.at(indice) + " >";
		} else if (option->tipo == OPC_SUBMENU){
			line = option->titulo + " >";
		} else if (option->tipo == OPC_INT){
			line = option->titulo;// + " " + ((OpcionInt *)option)->description + Constant::intToString(*((OpcionInt *)option)->valor);
		} else if (option->tipo == OPC_SHOW_TXT_VAL){
			line = option->titulo;
			value = ((OpcionTxtAndValue *)option)->valor;
		} else {
			line = option->titulo;
		}

		const int screenPos = i - this->iniPos;
        const int fontHeightRect = screenPos * face_h;
        const int lineBackground = -1;
        SDL_Color lineTextColor = i == this->curPos ? black : white;

        //Drawing a faded background selection rectangle
        if (i == this->curPos){
            int y = this->getY() + fontHeightRect;
            //Gaining some extra fps when the screen resolution is low
			SDL_Rect rectElem = {this->getX(), y, this->getW() - marginX, face_h};
            if (video_page->h >= 480){
				DrawRectAlpha(video_page, rectElem, bkgMenu, 190);
            } else {
                lineTextColor = white;
            }
			rect(video_page, rectElem.x - 1, rectElem.y - 1, rectElem.x + rectElem.w, rectElem.y + rectElem.h, bkgMenu);
        }
                
        Constant::drawTextTransparent(video_page, fontMenu, line.c_str(), this->getX(), 
                    this->getY() + fontHeightRect, lineTextColor, lineBackground);

		if (option->tipo == OPC_KEY && !((OpcionKey *)option)->description.empty()){
			std::string str = "";
			OpcionKey *opt = ((OpcionKey *)option);
			
			if (opt->changeAsked && SDL_GetTicks() - opt->lastTimeAsked < 4000){
				str = "Esperando pulsación de tecla en " + Constant::intToString( (5000 - (SDL_GetTicks() - opt->lastTimeAsked)) / 1000 ) + " s";
			} else if (opt->idx > -1){
				str = opt->description + Constant::intToString(opt->idx);
				if (opt->changeAsked) {
					opt->changeAsked = false;
					opt->lastTimeAsked = 0;
					status = NORMAL;
				} 				
			} else {
				str = "-";
			}

			int pixelDato;
			TTF_SizeText(fontMenu, str.c_str(), &pixelDato, NULL);
			Constant::drawTextTransparent(video_page, fontMenu, str.c_str(), this->getX() + this->getW() - marginX - pixelDato - 1, 
                    this->getY() + fontHeightRect, lineTextColor, lineBackground);

		} if (option->tipo == OPC_BOOLEANA){
			// 1. Extraer el valor y definir dimensiones base
			bool enabled = *((OpcionBool*)option)->valor;
			const int sw_h = face_h - 5;
			const int sw_w = 50;
			const int sw_x = getX() + getW() - marginX - sw_w;
			const int sw_y = getY() + fontHeightRect + 2;

			// 2. Dibujar el fondo del switch
			SDL_Rect baseRect = { sw_x, sw_y, sw_w, sw_h };
			SDL_FillRect(video_page, &baseRect, enabled ? iswitchenabled : iswitchdisabled);

			// 3. Calcular el thumb (botón interno) de forma relativa
			const int spacing = 4;
			const int size = sw_h - (spacing * 2);
			int thumbX = sw_x + (enabled ? (sw_w - size - spacing) : spacing);

			SDL_Rect thumbRect = { thumbX, sw_y + spacing, size, size };

			// 4. Dibujar el thumb según el estado
			if (enabled) {
				SDL_FillRect(video_page, &thumbRect, iblack);
			} else {
				// Usando los campos de thumbRect directamente para evitar sumas manuales
				rect(video_page, thumbRect.x, thumbRect.y, thumbRect.x + size, thumbRect.y + size, black);
				rect(video_page, thumbRect.x + 1, thumbRect.y + 1, thumbRect.x + size - 1, thumbRect.y + size - 1, black);
			}
			
		} else if (!value.empty()){
			int pixelDato;
			TTF_SizeText(fontMenu, value.c_str(), &pixelDato, NULL);
			Constant::drawTextTransparent(video_page, fontMenu, value.c_str(), this->getX() + this->getW() - marginX - pixelDato - 1, 
                    this->getY() + fontHeightRect, lineTextColor, lineBackground);
		}
    }

	drawAskMenu(video_page);
}

void GestorMenus::drawBooleanSwitch(int i, OpcionBool *opcion, SDL_Surface *video_page){

}

void GestorMenus::drawAskMenu(SDL_Surface *video_page){
	static const int iaskClBg = SDL_MapRGB(video_page->format, askClBg.r, askClBg.g, askClBg.b);
	static const int iaskClLine = SDL_MapRGB(video_page->format, askClLine.r, askClLine.g, askClLine.b);
	static const int iaskClTitle = SDL_MapRGB(video_page->format, askClTitle.r, askClTitle.g, askClTitle.b);
	static const int iaskClText = SDL_MapRGB(video_page->format, askClText.r, askClText.g, askClText.b);
	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);

	if (status == ASK_SAVESTATES){
		const int ask_w = 520;
		const int ask_h = 200;
		const int btn_h = 30;
		const int btn_w = 150;
		const int marginTitle = 10;

		SDL_Rect thumbRect = { this->w / 2 - ask_w / 2, this->h / 2 - ask_h / 2, ask_w, ask_h };
		SDL_Rect titleRect = { thumbRect.x, thumbRect.y, thumbRect.w, 40 };

		//Draw the popoup rectangle 
		SDL_FillRect(video_page, &thumbRect, iaskClBg);
		SDL_FillRect(video_page, &titleRect, iaskClTitle);
		rect(video_page, thumbRect.x, thumbRect.y, thumbRect.x + thumbRect.w, thumbRect.y + thumbRect.h, askClLine);
		rect(video_page, thumbRect.x-1, thumbRect.y-1, thumbRect.x + thumbRect.w +1, thumbRect.y + thumbRect.h +1, askClLine);

		Opcion* opt = menuAskSavestates->opciones[0];
		int face_h = TTF_FontLineSkip(fontMenu);
		//Draw the title
		Constant::drawTextTransparent(video_page, fontMenu, opt->titulo.c_str(), titleRect.x + marginTitle, 
                    titleRect.y + (titleRect.h - face_h) / 2, askClText, 0);

		if (opt->tipo == OPC_LISTA) {
			OpcionLista* l = (OpcionLista*)opt;
			if (l->items.size() <= 1){
				return;
			}

			int num = (int)l->items.size();
			const int freeSpace = (thumbRect.w - (l->items.size() * btn_w) - 2 * marginTitle) / (l->items.size() - 1);
			int pixelDato;
			SDL_Color clBtnSel = askClText;
			int iclBtnBgSel = iaskClLine;

			//Dibujamos los botones
			for (int i=0; i < l->items.size(); i++){
				SDL_Rect btnMidRect = { titleRect.x + 10 + ((btn_w  + freeSpace) * i), 
					thumbRect.y + titleRect.h + (thumbRect.h - titleRect.h) / 2, 
					btn_w, btn_h };

				TTF_SizeText(fontMenu, l->items[i].c_str(), &pixelDato, NULL);

				if (i == *(l->indice)){
					clBtnSel = askClTitle;
					iclBtnBgSel = iaskClText;
				} else {
					clBtnSel = askClText;
					iclBtnBgSel = iaskClLine;
				}

				SDL_Rect btnRect = { btnMidRect.x, btnMidRect.y - btn_h / 2, btn_w, btn_h};
				SDL_FillRect(video_page, &btnRect, iclBtnBgSel);
				rect(video_page, btnRect.x, btnRect.y, btnRect.x + btnRect.w, btnRect.y + btnRect.h, clBtnSel);

				Constant::drawTextTransparent(video_page, fontMenu, l->items[i].c_str(), 
					btnMidRect.x + (btn_w - pixelDato) / 2, 
					btnMidRect.y - face_h / 2, clBtnSel, 0);
			}
		}
	}
}

/**
*
*/
void GestorMenus::drawSavestateWithImage(int i, OpcionSavestate *opcion, SDL_Surface *video_page){
	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	TTF_Font *fontSmall = Fonts::getFont(Fonts::FONTSMALL);
	const int face_h = TTF_FontLineSkip(fontMenu);
	const int face_h_small = TTF_FontLineSkip(fontSmall);
    const int screenPos = i - this->iniPos;
    const int fontHeightRect = screenPos * face_h;
    const int lineBackground = -1;
    SDL_Color lineTextColor = i == this->curPos ? black : white;
	const std::string line = opcion->titulo;
	
	static std::string lastImagePath;
	std::string rutaSelected;
	auto option = this->menuActual->opciones.at(i);

	if (i == this->curPos){
		rutaSelected = opcion->file.filename;
        int y = this->getY() + fontHeightRect;
        //Gaining some extra fps when the screen resolution is low
		SDL_Rect rectElem = {this->getX(), y, this->getW() / 2 - 2 * marginX, face_h};
        if (video_page->h >= 480){
			DrawRectAlpha(video_page, rectElem, bkgMenu, 190);
        } else {
            lineTextColor = white;
        }
		//Drawing the selection menu
		rect(video_page, rectElem.x - 1, rectElem.y - 1, rectElem.x + rectElem.w, rectElem.y + rectElem.h, bkgMenu);
		
		//Drawing the modification date
		if (!opcion->file.modificationTime.empty()){
			//Drawing below the image
			Constant::drawTextTransparent(video_page, fontSmall, std::string("Último guardado: " + opcion->file.modificationTime).c_str(), imageMenu.getX(), 
                imageMenu.getY() + imageMenu.getH() + 2, white, 0);
		}
    } else {
		if (opcion->file.filename.empty()){
			lineTextColor = menuBars;
		}
	}

	//Drawing the text
    Constant::drawTextTransparent(video_page, fontMenu, line.c_str(), this->getX(), 
                this->getY() + fontHeightRect, lineTextColor, lineBackground);
	
	//Drawing the date besides the text
	Constant::drawTextTransparent(video_page, fontSmall, opcion->file.modificationTime.c_str(), this->getX() + 120, 
                this->getY() + fontHeightRect + face_h_small / 3, lineTextColor, lineBackground);

	//Drawing the image
	if (!rutaSelected.empty() && lastImagePath != rutaSelected){
		imageMenu.loadImage(opcion->file.dir + Constant::getFileSep() + opcion->file.filename + STATE_IMG_EXT);
		lastImagePath = opcion->file.filename;
	}

	if (!rutaSelected.empty()){
		imageMenu.printImage(video_page);
	}
	//rect(video_page, imageMenu.getX(), imageMenu.getY(), imageMenu.getX() + imageMenu.getW(), imageMenu.getY() + imageMenu.getH(), white);
}

/**
*
*/
int GestorMenus::getScreenNumLines(){
	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	int face_h = TTF_FontLineSkip(fontMenu);
    return face_h != 0 ? (int)std::floor((double)getH() / face_h) : 0;
}

/**
* 
*/
void GestorMenus::resetIndexPos(){
	if (menuActual != NULL){
		this->listSize = this->menuActual->opciones.size();
		this->maxLines = this->getScreenNumLines();
		/*To go to the bottom of the list*/
		//this->endPos = getListGames()->size();
		//this->iniPos = (int)getListGames()->size() >= this->maxLines ? getListGames()->size() - this->maxLines : 0;
		//this->curPos = this->endPos - 1;
		/*To go to the init of the list*/
		this->iniPos = 0;
		this->curPos = 0;
		this->endPos = (int)this->listSize > this->maxLines ? this->maxLines : this->listSize;
		this->pixelShift = 0;
		this->lastSel = -1;
		menuActual->seleccionado = 0;
	}
}

/**
void GestorMenus::nextPos(){
	this->navegar(1);
	this->curPos = menuActual->seleccionado;
}

void GestorMenus::prevPos(){
	this->navegar(-1);
	this->curPos = menuActual->seleccionado;
}
*/

// Lógica de navegación Arriba/Abajo
void GestorMenus::navegar(int dir) { // -1 o 1
    if (!menuActual || status == POLLING_INPUTS || status == ASK_SAVESTATES) return;

    /*int num = (int)menuActual->opciones.size();
	if (num > 0){
		menuActual->seleccionado = (menuActual->seleccionado + dir + num) % num;
	}*/

	if (dir > 0){
		if (this->curPos < this->listSize - 1){
			this->curPos++;
			menuActual->seleccionado = this->curPos;
			int posCursorInScreen = this->curPos - this->iniPos;
		
			if (posCursorInScreen > this->maxLines - 1){
				this->iniPos++;
				this->endPos++;
			}
			this->pixelShift = 0;
			this->lastSel = -1;
		}
	} else if (dir < 0){
		if (this->curPos > 0){
			this->curPos--;
			menuActual->seleccionado = this->curPos;
			if (this->curPos < this->iniPos && this->curPos >= 0){
				this->iniPos--;
				this->endPos--;
			}
			this->pixelShift = 0;
			this->lastSel = -1;
		}
	}

}

void GestorMenus::nextPos(){
    navegar(1);
}

void GestorMenus::prevPos(){
    navegar(-1);
}


void GestorMenus::clearSelectedText(){
    if (imgText != NULL){
		SDL_FreeSurface(imgText);
        imgText = NULL;
    }
}