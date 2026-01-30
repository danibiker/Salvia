#include <SDL.h>
#include <io/joystick.h>
#include <io/joymapper.h>
#include <io/hotkeys.h>
#include <io/filelist.h>
#include <const/constant.h>

t_joy_retro_inputs Joystick::buttonsMapperLibretro[MAX_PLAYERS];
t_joy_inputs Joystick::buttonsMapperFrontend;

Joystick::Joystick(){
	ignoreButtonRepeats = false;
	this->w = 0;
    this->h = 0;
	//lastSelectPress = 0;
	clearEvento(&lastEvento);
    gestorCursor = new CursorGestor();
	setCursor(cursor_hidden);
	mPrevAxisValues = NULL;
	mPrevHatValues = NULL;

	for (int i=0; i < MAX_PLAYERS; i++){
		g_joysticks[i] = NULL;
		for (int j=0; j < RETRO_DEVICE_ID_JOYPAD_R3 + 1; j++){
			g_joy_state[i][j] = false;
			g_axis_state[i][j] = false;
		}
	}

	resetButtonsFrontend();
}

void Joystick::resetButtonsCore(){
	for (int i=0; i < MAX_PLAYERS; i++){
		for (int j=0; j < RETRO_DEVICE_ID_JOYPAD_R3 + 1; j++){
			g_joy_state[i][j] = false;
			g_axis_state[i][j] = false;
		}
	}
}

void Joystick::resetButtonsFrontend(){
	for (int i=0; i < MAXJOYBUTTONS + 1; i++){
		g_joy_frontend_state[0][i] = false;
	}
}

Joystick::~Joystick(){
	close_joysticks();
	if (buttonsMapperFrontend.buttons) delete [] buttonsMapperFrontend.buttons;
	if (buttonsMapperFrontend.axis) delete [] buttonsMapperFrontend.axis;
	if (buttonsMapperFrontend.hats) delete [] buttonsMapperFrontend.hats;
	if (mPrevAxisValues) delete [] mPrevAxisValues;
	if (mPrevHatValues) delete [] mPrevHatValues;
}

/**
* Mostramos un cursor vacio para poder ocultar el cursor y evitar el problema de
* llamar a SDL_ShowCursor(SDL_DISABLE); para ocultarlo. Resultaba que se movia el cursor
* a una posicion no deseada
*/
void Joystick::setCursor(int cursor){
    SDL_SetCursor(gestorCursor->getCursor(cursor));
    actualCursor = cursor;
}

bool Joystick::init_all_joysticks() {
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    mNumJoysticks = SDL_NumJoysticks();
    
	if (mNumJoysticks <= 0){
		return false;
	}
	
	int axis = 0;
    int hats = 0;
	int buttons = 0;

	mPrevAxisValues = new std::map<int, int>[MAX_PLAYERS];
	mPrevHatValues = new std::map<int, int>[MAX_PLAYERS];

	std::vector<std::string> joynamesOnPort = loadControllerPorts();
	std::map<std::string, t_joy_retro_inputs> masterJoyList = loadButtonsRetroList();

    // Abrir todos los mandos disponibles hasta el límite de jugadores
    for (int joyId = 0; joyId < MAX_PLAYERS; joyId++) {
        g_joysticks[joyId] = SDL_JoystickOpen(joyId);
        
		if (g_joysticks[joyId]) {
			buttonsMapperFrontend.joyName = Constant::Trim(SDL_JoystickName(joyId));
			buttonsMapperLibretro[joyId].joyName = buttonsMapperFrontend.joyName;
			LOG_DEBUG("Mando %d abierto: %s\n", joyId, buttonsMapperFrontend.joyName.c_str());
			axis = SDL_JoystickNumAxes(g_joysticks[joyId]);
			hats = SDL_JoystickNumHats(g_joysticks[joyId]);
			buttons = SDL_JoystickNumButtons(g_joysticks[joyId]);
        } else {
			axis = MAX_ANALOG_AXIS;
			hats = RETRO_DEVICE_ID_JOYPAD_R3 + 1;
			buttons = 10;
		}

        //cout << "hay " + Constant::TipoToStr(axis) + " axis en el joystick: " + Constant::TipoToStr(i) << endl;
        for(int k = 0; k < axis; k++){
            mPrevAxisValues[joyId][k] = 0;
        }
        for(int k = 0; k < hats; k++){
            mPrevHatValues[joyId][k] = 0;
        }

		//Almacenamos las posiciones mapeadas para cada boton del emulador
		loadButtonsEmupad(joyId, joynamesOnPort, masterJoyList);
		
		if (joyId == 0){
			//creamos las posiciones mapeadas para cada boton del frontend 
			//solo para el primer mando detectado
			buttonsMapperFrontend.buttons = new int [buttons];
			buttonsMapperFrontend.axis = new int [axis * 2];
			buttonsMapperFrontend.hats = new int [MAX_HAT_POSITIONS];

			std::fill(buttonsMapperFrontend.buttons, buttonsMapperFrontend.buttons + buttons, -1);
			std::fill(buttonsMapperFrontend.axis, buttonsMapperFrontend.axis + (axis * 2), -1);
			std::fill(buttonsMapperFrontend.hats, buttonsMapperFrontend.hats + MAX_HAT_POSITIONS, -1);

			buttonsMapperFrontend.nButtons = buttons;
			buttonsMapperFrontend.nAxis = axis;
			buttonsMapperFrontend.nHats = MAX_HAT_POSITIONS;
		}
    }

	return loadButtonsFrontend(Constant::getAppDir() + Constant::getFileSep() + "joystick.ini");
}

/**
*
*/
void Joystick::loadButtonsEmupad(int joyId, std::vector<std::string>& joyNamesOnPort, std::map<std::string, t_joy_retro_inputs>& masterJoyList) {

	if (joyId >= 0 && joyId < (int)joyNamesOnPort.size()) {
		std::string nameToFind = buttonsMapperLibretro[joyId].joyName;
		int num_port_buttons = sizeof(configurablePortButtons) / sizeof(configurablePortButtons[0]);

		if (masterJoyList.count(nameToFind) > 0) {
            // 3. Copia de la configuración
			t_joy_retro_inputs inputFound =  masterJoyList[nameToFind];
			
			for (int i=0; i < inputFound.nButtons; i++){
				buttonsMapperLibretro[joyId].setButton(i, inputFound.buttons[i]);
			}

			for (int i=0; i < inputFound.nAxis; i++){
				buttonsMapperLibretro[joyId].setAxis(i, inputFound.axis[i]);
			}

			for (int i=0; i < inputFound.nHats; i++){
				buttonsMapperLibretro[joyId].setHat(i, inputFound.hats[i]);
			}
			buttonsMapperLibretro[joyId].axisAsPad = inputFound.axisAsPad;
            LOG_INFO("Configuración cargada para el puerto %d: %s", joyId, nameToFind.c_str());

        } else {
			int nButtons = SDL_JoystickNumButtons(g_joysticks[joyId]);
			int nAxis = SDL_JoystickNumAxes(g_joysticks[joyId]);

			for (int i=0; i < num_port_buttons && i < nButtons; i++){
				buttonsMapperLibretro[joyId].setButton(i, configurablePortButtons[i]);
			}

			if (nAxis >= 1){
				buttonsMapperLibretro[joyId].setAxis(0, RETRO_DEVICE_ID_JOYPAD_LEFT);
				buttonsMapperLibretro[joyId].setAxis(1, RETRO_DEVICE_ID_JOYPAD_RIGHT);
			}
			
			if (nAxis >= 2){
				buttonsMapperLibretro[joyId].setAxis(2, RETRO_DEVICE_ID_JOYPAD_UP);
				buttonsMapperLibretro[joyId].setAxis(3, RETRO_DEVICE_ID_JOYPAD_DOWN);
			}

			if (nAxis >= 3){
				buttonsMapperLibretro[joyId].setAxis(4, RETRO_DEVICE_ID_JOYPAD_R2);
				buttonsMapperLibretro[joyId].setAxis(5, RETRO_DEVICE_ID_JOYPAD_L2);
			}

			buttonsMapperLibretro[joyId].setHat(SDL_HAT_UP, RETRO_DEVICE_ID_JOYPAD_UP);
			buttonsMapperLibretro[joyId].setHat(SDL_HAT_RIGHT, RETRO_DEVICE_ID_JOYPAD_RIGHT);
			buttonsMapperLibretro[joyId].setHat(SDL_HAT_DOWN, RETRO_DEVICE_ID_JOYPAD_DOWN);
			buttonsMapperLibretro[joyId].setHat(SDL_HAT_LEFT, RETRO_DEVICE_ID_JOYPAD_LEFT);

			buttonsMapperLibretro[joyId].axisAsPad = true;

			LOG_INFO("Configuración por defecto para el puerto %d: %s", joyId, nameToFind.c_str());
		}
	}
}

void Joystick::cargarValoresEnArray(int *&arr, std::string str, int maxValues){
	std::vector<std::string> v = Constant::splitChar(str, ',');
	for (int i=0; i < v.size() && i < maxValues; i++){
		arr[i] = Constant::strToTipo<int>(v[i]);
	}
}

bool Joystick::loadButtonsFrontend(std::string rutaIni){
	std::vector<std::string> fileConfigJoystick;
	FileList::cargarVector(rutaIni, fileConfigJoystick);
	int nLinesProcessed = 0;

	if (fileConfigJoystick.size() > 0){
		std::string linea = "";
		bool foundFrontend = false;
		std::size_t pos = 0;
        for (unsigned int i=0; i<fileConfigJoystick.size(); i++){
            linea = fileConfigJoystick.at(i);
			LOG_DEBUG("Linea %s", linea.c_str());

			if (foundFrontend && (pos = linea.find("btns=")) != std::string::npos){
				linea = linea.substr(pos + 5);	
				cargarValoresEnArray(buttonsMapperFrontend.buttons, linea, buttonsMapperFrontend.nButtons);
				nLinesProcessed++;
			} else if (foundFrontend && (pos = linea.find("hats=")) != std::string::npos){
				linea = linea.substr(pos + 5);	
				cargarValoresEnArray(buttonsMapperFrontend.hats, linea, buttonsMapperFrontend.nHats);
				nLinesProcessed++;
			} else if (foundFrontend && (pos = linea.find("axis=")) != std::string::npos){
				linea = linea.substr(pos + 5);	
				cargarValoresEnArray(buttonsMapperFrontend.axis, linea, buttonsMapperFrontend.nAxis * 2);
				nLinesProcessed++;
			} else if (linea == "[FRONTEND]"){
				foundFrontend = true;
			}
		}
	}
	return nLinesProcessed < 3;
}

void Joystick::saveButtonsFrontend(std::string rutaIni){
	std::vector<std::string> fileConfigJoystick;

	fileConfigJoystick.push_back("[FRONTEND]");
	fileConfigJoystick.push_back("name=" + buttonsMapperFrontend.joyName);

	std::string str = "btns=";
	for (int i=0; i < buttonsMapperFrontend.nButtons; i++){
		str += Constant::intToString(buttonsMapperFrontend.buttons[i]) + (i < buttonsMapperFrontend.nButtons-1 ? "," : "");
	}
	fileConfigJoystick.push_back(str);

	str = "hats=";
	for (int i=0; i < buttonsMapperFrontend.nHats; i++){
		str += Constant::intToString(buttonsMapperFrontend.hats[i]) + (i < buttonsMapperFrontend.nHats-1 ? "," : "");
	}
	fileConfigJoystick.push_back(str);

	str = "axis=";
	for (int i=0; i < buttonsMapperFrontend.nAxis * 2; i++){
		str += Constant::intToString(buttonsMapperFrontend.axis[i]) + (i < buttonsMapperFrontend.nAxis * 2 -1? "," : "");
	}
	fileConfigJoystick.push_back(str);
	FileList::guardarVector(rutaIni, fileConfigJoystick);
}

int Joystick::cargarValoresEnArray(t_retro_input *&arr, std::string str){
	std::vector<std::string> v = Constant::splitChar(str, ',');
	int maxValues = v.size();
	arr = new t_retro_input [maxValues];

	for (int i=0; i < v.size(); i++){
		arr[i].joy = Constant::strToTipo<int>(v[i]);
	}
	return maxValues;
}

std::vector<std::string> Joystick::loadControllerPorts() {
    std::vector<std::string> ports(MAX_PLAYERS, "None"); 
    
    std::string path = Constant::getAppDir() + Constant::getFileSep() + "retropad.ini";
    std::ifstream file(path.c_str());
    std::string linea;
    bool foundports = false;
    int portNumber = -1; // Empezamos en -1 para validar que se leyó un índice

    while (std::getline(file, linea)) {
        linea = Constant::Trim(linea);
        if (linea.empty()) continue;

        // Si encontramos otra sección, dejamos de buscar en RETROPAD
        if (linea[0] == '[' ) {
            if (linea == "[RETROPAD]") {
                foundports = true;
                continue;
            } else if (foundports) {
                break; // Ya terminamos con nuestra sección
            }
        }

        if (!foundports) continue;

        size_t pos = linea.find('=');
        if (pos == std::string::npos) continue;

        std::string key = Constant::Trim(linea.substr(0, pos));
        std::string val = Constant::Trim(linea.substr(pos + 1));

        if (key == "indx") {
            portNumber = Constant::strToTipo<int>(val);
        } else if (key == "name") {
            // Verificamos que el índice sea válido (0-3) antes de asignar
            if (portNumber >= 0 && portNumber < (int)ports.size()) {
                ports[portNumber] = val;
            }
        }
    }
    return ports;
}

/**
*
*/
std::map<std::string, t_joy_retro_inputs> Joystick::loadButtonsRetroList() {
    std::map<std::string, t_joy_retro_inputs> joyList;
    std::string path = Constant::getAppDir() + Constant::getFileSep() + "retropad.ini";
    
    std::ifstream file(path.c_str());
    std::string linea;
    t_joy_retro_inputs currentJoy;
    bool foundList = false;

    while (std::getline(file, linea)) {
        // Trim simple (quitar \r o espacios)
		linea = Constant::Trim(linea);

        if (linea == "[RETROPAD_LIST]") { foundList = true; continue; }
        if (linea == "[RETROPAD]") break;
        if (!foundList) continue;

        size_t pos = linea.find('=');
        if (pos == std::string::npos) continue;

        std::string key = Constant::Trim(linea.substr(0, pos));
        std::string val = Constant::Trim(linea.substr(pos + 1));

        if (key == "name") {
            currentJoy.joyName = val;
        } else if (key == "btns") {
			cargarValoresEnArray(currentJoy.buttons, val, currentJoy.nButtons);
        } else if (key == "hats") {
			cargarValoresEnArray(currentJoy.hats, val, currentJoy.nHats);
        } else if (key == "axis") {
            cargarValoresEnArray(currentJoy.axis, val, currentJoy.nAxis);
        } else if (key == "anal") {
			currentJoy.axisAsPad = val == "1";
            joyList[currentJoy.joyName] = currentJoy; // Guardar al completar el bloque
        } else if (key == "htks") {
			std::vector<std::string> v = Constant::splitChar(val, ',');
			if (v.size() > 0){
				//El primer elemento es el que activa las hotkeys
				hotkeys.g_modifierButton = Constant::strToTipo<int>(v[0]);
				for (int i = 1; i < v.size() && i < HK_MAX + 1; i++){
					hotkeys.g_hotkeys[i - 1].triggerButton = Constant::strToTipo<int>(v[i]);
				}
			}
		}
    }
    return joyList;
}

void Joystick::addJoyToList(std::vector<std::string> &fileConfigJoystick, t_joy_retro_inputs& retroInputs){
	fileConfigJoystick.push_back("name=" + Constant::Trim(retroInputs.joyName));

	std::string str = "btns=";
	for (int i=0; i < retroInputs.nButtons; i++){
		std::string istr = Constant::intToString(retroInputs.buttons[i]);
		str += istr + (i < retroInputs.nButtons-1 ? "," : "");
	}
	fileConfigJoystick.push_back(str);

	str = "hats=";
	for (int i=0; i < retroInputs.nHats; i++){
		str += Constant::intToString(retroInputs.hats[i]) + (i < retroInputs.nHats-1 ? "," : "");
	}
	fileConfigJoystick.push_back(str);

	str = "axis=";
	for (int i=0; i < retroInputs.nAxis; i++){
		str += Constant::intToString(retroInputs.axis[i]) + (i < retroInputs.nAxis-1 ? "," : "");
	}
	fileConfigJoystick.push_back(str);
	fileConfigJoystick.push_back("anal=" + std::string(retroInputs.axisAsPad ? "1" : "0"));
}

std::string Joystick::searchNewName(std::map<std::string, t_joy_retro_inputs>& retroInputs, std::string previousName){
	int i=1;
	int notFound = false;
	std::string newName = "";

	int iniPos = 0;
	int endPos = 0;
	if ((iniPos = previousName.find_last_of("(")) != std::string::npos && (endPos = previousName.find_last_of(")")) != std::string::npos
		&& iniPos < endPos){
		std::string lastNumber = previousName.substr(iniPos + 1, endPos - iniPos - 1);
		if (Constant::esNumerico(lastNumber)){
			i = Constant::strToTipo<int>(lastNumber);
			previousName = previousName.substr(0, iniPos);
		}
	}

	while (!notFound){
		newName = previousName + "(" + Constant::intToString(i) + ")";
		notFound = retroInputs.find(newName) == retroInputs.end();
		i++;
	}
	return newName;
}

/**
*
*/
std::string Joystick::saveButtonsRetro(){
	std::vector<std::string> fileConfigJoystick;

	fileConfigJoystick.push_back("[RETROPAD_LIST]");
	std::map<std::string, t_joy_retro_inputs> joyList;

	for (int p=0; p < MAX_PLAYERS; p++){
		if (buttonsMapperLibretro[p].joyName.empty())
			continue;

		auto elemFound = joyList.find(buttonsMapperLibretro[p].joyName);
		if (elemFound != joyList.end()){
			//Si se ha encontrado el mismo elemento, comprobamos si son iguales
			if (elemFound->second.equals(buttonsMapperLibretro[p])){
				//Si tiene la misma configuracion, no hacemos nada
				continue;
			} else {
				//Si tiene una configuracion distinta, debemos generar un nuevo nombre
				buttonsMapperLibretro[p].joyName = searchNewName(joyList, buttonsMapperLibretro[p].joyName);
			}
		}
		addJoyToList(fileConfigJoystick, buttonsMapperLibretro[p]);

		if (p == 0){
			std::string strHotkeys = "htks=";
			strHotkeys += Constant::intToString(hotkeys.g_modifierButton) + ",";
			for (int i=0; i < hotkeys.g_hotkeys.size(); i++){
				strHotkeys += Constant::intToString(hotkeys.g_hotkeys[i].triggerButton) +  (i < hotkeys.g_hotkeys.size() - 1 ? "," : "");
			}
			fileConfigJoystick.push_back(strHotkeys);
		}

		fileConfigJoystick.push_back("");
		LOG_DEBUG("Adding joy to config: %s", buttonsMapperLibretro[p].joyName.c_str());
		joyList[buttonsMapperLibretro[p].joyName] = buttonsMapperLibretro[p]; // Inserts or updates
	}

	std::map<std::string, t_joy_retro_inputs> joyListPrev = loadButtonsRetroList();
	for (auto it = joyListPrev.begin(); it != joyListPrev.end(); ++it) {
		auto elemFound = joyList.find(it->first);
		if (elemFound == joyList.end()){
			LOG_DEBUG("Adding joy to config: %s", it->first.c_str());
			addJoyToList(fileConfigJoystick, it->second);
		} else {
			//Si hemos encontrado un joystick con el mismo nombre
			//comprobamos si tiene las mismas asignaciones
			if (!elemFound->second.equals(it->second)){
				it->second.joyName = searchNewName(joyList, it->second.joyName);
				addJoyToList(fileConfigJoystick, it->second);
			}
		}
	}

	fileConfigJoystick.push_back("");
	fileConfigJoystick.push_back("[RETROPAD]");
	for (int p=0; p < MAX_PLAYERS; p++){
		fileConfigJoystick.push_back("indx=" + Constant::intToString(p));
		fileConfigJoystick.push_back("name=" + buttonsMapperLibretro[p].joyName);
		fileConfigJoystick.push_back("");
	}
	
	std::string rutaGuardado = Constant::getAppDir() + Constant::getFileSep() + "retropad.ini";
	FileList::guardarVector(rutaGuardado, fileConfigJoystick);
	return rutaGuardado;
}

void Joystick::resetAllValues(){
	int axis = 0;
    int hats = 0;
	int buttons = 0;

	// Abrir todos los mandos disponibles hasta el límite de jugadores
    for (int i = 0; i < mNumJoysticks && i < MAX_PLAYERS; i++) {
		axis = SDL_JoystickNumAxes(g_joysticks[i]);
        hats = SDL_JoystickNumHats(g_joysticks[i]);
		buttons = SDL_JoystickNumButtons(g_joysticks[i]);
        //cout << "hay " + Constant::TipoToStr(axis) + " axis en el joystick: " + Constant::TipoToStr(i) << endl;
        for(int k = 0; k < axis; k++){
            mPrevAxisValues[i][k] = 0;
        }
        for(int k = 0; k < hats; k++){
            mPrevHatValues[i][k] = 0;
        }
		
		for (int j=0; j < RETRO_DEVICE_ID_JOYPAD_R3 + 1; j++){
			g_joy_state[i][j] = false;
			g_axis_state[i][j] = false;
		}
    }

	lastEvento.keyjoydown = false;

	clearEvento(&evento);
	clearEvento(&lastEvento);
}


void Joystick::close_joysticks() {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (g_joysticks[i]) {
			SDL_JoystickClose(g_joysticks[i]);
			g_joysticks[i] = NULL;
		}
	}
}

tEvento Joystick::WaitForKey(SDL_Surface* screen){
	static unsigned long lastClick = 0;
    static unsigned long lastKeyDown = 0;
	static unsigned long longKeyDown = 0;
    static unsigned long retrasoTecla = KEYRETRASO;
    static unsigned long lastMouseMove = 0;
    static t_region mouseRegion = {0,0,0,0};

    clearEvento(&evento);
    SDL_Event event;

	while( SDL_PollEvent( &event ) ){
		 switch( event.type ){
            case SDL_VIDEORESIZE:
                if (!(screen->flags & SDL_FULLSCREEN)){
                    screen = SDL_SetVideoMode (event.resize.w, event.resize.h, video_bpp, video_flags);
                }
                this->w = event.resize.w;
                this->h = event.resize.h;
                evento.resize = true;
                evento.width = event.resize.w;
                evento.height = event.resize.h;
                break;
            case SDL_JOYBUTTONDOWN: // JOYSTICK/GP2X buttons
                if (event.jbutton.button >= 0 && event.jbutton.button < MAXJOYBUTTONS){
					evento.joy = buttonsMapperFrontend.buttons[event.jbutton.button];
					evento.sdljoybtn = event.jbutton.button;
                    evento.isJoy = true;
                    evento.keyjoydown = true;
                    lastEvento = evento;    //Guardamos el ultimo evento que hemos lanzado desde el teclado
                    lastKeyDown = SDL_GetTicks();  //reseteo del keydown
					longKeyDown = lastKeyDown;
                }
                break;
            case SDL_JOYBUTTONUP:
                lastEvento = evento;
                evento.joy = buttonsMapperFrontend.buttons[event.jbutton.button];
				evento.sdljoybtn = event.jbutton.button;
				evento.isJoy = true;
				evento.keyjoydown = false;
				longKeyDown = 0;
                break;
            case SDL_JOYHATMOTION:
                mPrevHatValues[event.jhat.which][event.jhat.hat] = event.jhat.value;
                evento.isJoy = true;
                evento.joy = buttonsMapperFrontend.hats[event.jhat.value];

				if (event.jhat.value == 0) {
					evento.keyjoydown = false;
				} else {
                    evento.keyjoydown = true;
                    lastKeyDown = SDL_GetTicks();  //reseteo del keydown
                }

                lastEvento = evento;    //Guardamos el ultimo evento que hemos lanzado

                break;
            case SDL_JOYAXISMOTION:
                if((abs(event.jaxis.value) > DEADZONE) != (abs(mPrevAxisValues[event.jaxis.which][event.jaxis.axis]) > DEADZONE))
                {
                    evento.isJoy = true;
                    if(abs(event.jaxis.value) <= DEADZONE){
                        evento.keyjoydown = false;
                    } else {
                        evento.keyjoydown = true;
                        lastKeyDown = SDL_GetTicks();  //reseteo del keydown
                    }

					if (abs(event.jaxis.value) > DEADZONE) {
						// 0 si es negativo (Izquierda/Arriba), 1 si es positivo (Derecha/Abajo)
						int isPositive = (event.jaxis.value > 0);
						int buttonIdx = (event.jaxis.axis * 2) + isPositive;

						// Asignamos el valor mapeado al evento
						evento.joy = buttonsMapperFrontend.axis[buttonIdx];

						LOG_DEBUG("Eje: %d, Boton virtual: %d, Mapeo: %d", 
								   event.jaxis.axis, buttonIdx, evento.joy);
					} else {
						// CENTRO: El eje ha vuelto a la zona muerta
					}
                    lastEvento = evento;    //Guardamos el ultimo evento que hemos lanzado desde el teclado
                }

                mPrevAxisValues[event.jaxis.which][event.jaxis.axis] = event.jaxis.value;

                break;
            case SDL_KEYDOWN: // PC buttons
                evento.key = event.key.keysym.sym;
                evento.keyMod = event.key.keysym.mod;
                evento.unicode = event.key.keysym.unicode;
                evento.isKey = true;
                evento.keyjoydown = true;
                if (evento.keyMod & KMOD_LCTRL && evento.key == SDLK_c) 
					evento.quit = true;
                lastEvento = evento;    //Guardamos el ultimo evento que hemos lanzado desde el teclado
                lastKeyDown = SDL_GetTicks();  //reseteo del keydown
                break;
            case SDL_KEYUP: // PC button keyup
                lastEvento = evento;
                break;
            case SDL_MOUSEBUTTONDOWN: // Mouse buttons SDL_BUTTON_LEFT, SDL_BUTTON_MIDDLE, SDL_BUTTON_RIGHT, SDL_BUTTON_WHEELUP, SDL_BUTTON_WHEELDOWN
                evento.mouse = event.button.button;
                evento.mouse_x = event.button.x;
                evento.mouse_y = event.button.y;
                evento.mouse_state = event.button.state;
                evento.isMouse = true;
                if (SDL_GetTicks() - lastClick < DBLCLICKSPEED){
                   evento.isMousedblClick = true;
                   lastClick = SDL_GetTicks() - DBLCLICKSPEED;  //reseteo del dobleclick
                } else {
                    lastClick = SDL_GetTicks();
                }
                mouseRegion.selX = event.button.x;
                mouseRegion.selY = event.button.y;
                evento.region = mouseRegion;
                break;
            case SDL_MOUSEBUTTONUP:
                evento.mouse = event.button.button;
                evento.mouse_x = event.button.x;
                evento.mouse_y = event.button.y;
                evento.mouse_state = event.button.state;
                evento.isMouse = true;
                evento.isRegionSelected = false;
                break;
            case SDL_MOUSEMOTION:
                evento.mouse = event.button.button;
                evento.mouse_x = event.button.x;
                evento.mouse_y = event.button.y;
                evento.mouse_state = event.button.state;
                evento.isMouseMove = true;
                lastMouseMove = SDL_GetTicks();
                if (actualCursor == cursor_hidden && evento.mouse_state != SDL_PRESSED
                    && evento.mouse != MOUSE_BUTTON_LEFT){
                     setCursor(cursor_arrow);
                }

                if (evento.mouse == MOUSE_BUTTON_LEFT){
                    mouseRegion.selW = event.button.x - mouseRegion.selX;
                    mouseRegion.selH = event.button.y - mouseRegion.selY;
                    evento.isRegionSelected = true;
                } else {
                    mouseRegion.selW = 0;
                    mouseRegion.selH = 0;
                    evento.isRegionSelected = false;
                }
                evento.region = mouseRegion;
//                Traza::print("mouse motion. selected: " + string(evento.isRegionSelected ? "S" : "N")
//                + " mouse state: " + Constant::TipoToStr(evento.mouse_state)
//                + " evt mouse: " + Constant::TipoToStr(evento.mouse)
//                + " selW: " + Constant::TipoToStr(mouseRegion.selW)
//                + " selH: " + Constant::TipoToStr(mouseRegion.selH),
//                        W_DEBUG);
                break;
            case SDL_QUIT:
                evento.quit = true;
                break;
            default :
                break;
        }
    } 
	//else {
    //    clearEvento(&lastEvento);
	//	printf("clearEvento\n");
    //}

    unsigned long now = SDL_GetTicks();

    //En algunas ocasiones se repiten eventos. Con este flag los controlamos
    if (ignoreButtonRepeats){
        lastEvento.keyjoydown = false;
        ignoreButtonRepeats = false;
    }
	
	if (lastEvento.keyjoydown == true){
		if (longKeyDown > 0 && now - longKeyDown > LONGKEYTIMEOUT && lastEvento.joy >= 0 && lastEvento.joy < MAXJOYBUTTONS){
			LOG_DEBUG("Long press detected for key %d\n", lastEvento.joy);
			lastEvento.longKeyPress[lastEvento.joy] = true;
			longKeyDown = 0;
		} 

		//printf("now %d vs %d\n", now, lastKeyDown + KEYDOWNSPEED + retrasoTecla);
        if (now > lastKeyDown + KEYDOWNSPEED + retrasoTecla){
			LOG_DEBUG("Repeating key %d\n", lastEvento.joy);
            lastKeyDown = SDL_GetTicks();
            evento = lastEvento;
            retrasoTecla = 0;
        }
    } else {
        retrasoTecla = KEYRETRASO;
    }

    if (now > lastMouseMove + MOUSEVISIBLE && actualCursor != cursor_hidden){
        setCursor(cursor_hidden);
    }

    return evento;
}

void Joystick::clearEvento(tEvento *evento){
    evento->key = INT_MIN;
    evento->joy = INT_MIN;
    evento->isJoy = false;
    evento->isKey = false;
    evento->isMouse = false;
    evento->keyMod = INT_MIN;
    evento->unicode = INT_MIN;
    evento->resize = false;
    evento->quit = false;
    evento->isMousedblClick = false;
    evento->isMouseMove = false;
    evento->isRegionSelected = false;
    evento->mouse_state = INT_MIN;
    evento->mouse = INT_MIN;
    evento->mouse_x = 0;
    evento->mouse_y = 0;
    evento->keyjoydown = false;
    evento->width = 0;
    evento->height = 0;
	//evento->lastSelectPress = 0;
	//evento->longKeyPress = false;
	memset(evento->longKeyPress, 0, sizeof(evento->longKeyPress));
    //t_region region;
}

HOTKEYS_LIST Joystick::findHotkey(){
	return hotkeys.ProcesarHotkeys(g_joy_frontend_state[0]);
}