#include <SDL.h>
#include <io/joystick.h>
#include <io/hotkeys.h>
#include <io/filelist.h>
#include <io/dirutil.h>
#include <const/constant.h>
#include <io/cfgloader.h>
#include <cmath>

Joystick::Joystick(){
	ignoreButtonRepeats = false;
	this->w = 0;
    this->h = 0;
    gestorCursor = new CursorGestor();
	setCursor(cursor_hidden);
	hotkeys = new Hotkeys(&this->inputs);
	memset(startHoldFrames, 0, sizeof(startHoldFrames));
}

Joystick::~Joystick(){
	close_joysticks();
	delete hotkeys;
}

/**
*
*/
bool Joystick::init_all_joysticks() {
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    mNumJoysticks = SDL_NumJoysticks();
	
	for (int joyId = 0; joyId < MAX_PLAYERS; joyId++) {
        g_joysticks[joyId] = SDL_JoystickOpen(joyId);
		if (g_joysticks[joyId]) {
			inputs.names[joyId] = Constant::Trim(SDL_JoystickName(joyId));
			//inputs.names[joyId] = "Retropad Default";
			//Loading default values
			inputs.axisAsPad[joyId] = true;
			//Setting mappers for the frontend
			configMapperFrontend(inputs.mapperFrontend, joyId);
			//Setting mappers for the core's emulator
			configMapperRetro(inputs.mapperCore, joyId);
		}
	}
	std::string ruta = Constant::getAppDir() + Constant::getFileSep() + "retropad.ini";
	loadButtonsRetro(ruta);
	return true;
}

/**
*
*/
void Joystick::configMapperFrontend(t_joy_mapper& mapper, int joyId){
	int hatsDirections = sizeof(configurableSdlHats) / sizeof(configurableSdlHats[0]);
	int arrNButtons = sizeof(configurableFrontButtons) / sizeof(configurableFrontButtons[0]);
	int arrNAxis = sizeof(configurableSdlFrontAxis) / sizeof(configurableSdlFrontAxis[0]);

	int naxis = SDL_JoystickNumAxes(g_joysticks[joyId]) * 2;
	int nhats = SDL_JoystickNumHats(g_joysticks[joyId]);
	int nbuttons = SDL_JoystickNumButtons(g_joysticks[joyId]);

	for (int btn=0; btn < nbuttons && btn < arrNButtons; btn++){
		mapper.setBtnFromSdl(joyId, btn, configurableFrontButtons[btn]);
	}

	if (nhats >= 1){
		for (int hatDir = 0; hatDir < hatsDirections; hatDir++){
			mapper.setHatFromSdl(joyId, (int)pow((double)2, hatDir), configurableSdlFrontHats[hatDir]);
		}
	}

	for (int axis=0; axis < naxis && axis < arrNAxis; axis++){
		mapper.setAxisFromSdl(joyId, axis, configurableSdlFrontAxis[axis]);
	}
}

/**
*
*/
void Joystick::configMapperRetro(t_joy_mapper& mapper, int joyId){
	int hatsDirections = sizeof(configurableSdlHats) / sizeof(configurableSdlHats[0]);
	int arrNButtons = sizeof(configurablePortButtons) / sizeof(configurablePortButtons[0]);
	int arrNAxis = sizeof(configurableSdlAxis) / sizeof(configurableSdlAxis[0]);

	int naxis = SDL_JoystickNumAxes(g_joysticks[joyId]) * 2; //Cada eje tiene dos direcciones
	int nhats = SDL_JoystickNumHats(g_joysticks[joyId]);
	int nbuttons = SDL_JoystickNumButtons(g_joysticks[joyId]);

	for (int btn=0; btn < nbuttons && btn < arrNButtons; btn++){
		mapper.setBtnFromSdl(joyId, btn, configurablePortButtons[btn]);
	}

	if (nhats >= 1){
		for (int hatDir = 0; hatDir < hatsDirections; hatDir++){
			mapper.setHatFromSdl(joyId, (int)pow((double)2, hatDir), configurableSdlHats[hatDir]);
		}
	}

	for (int axis=0; axis < naxis && axis < arrNAxis; axis++){
		mapper.setAxisFromSdl(joyId, axis, configurableSdlAxis[axis]);
	}
}

/**
*
*/
std::string Joystick::saveButtonsRetroGame() {
	if (!romPaths.rompath.empty()){
		dirutil dir;
		std::string rutaGuardado = dir.getFolder(romPaths.rompath) + Constant::getFileSep() + dir.getFileNameNoExt(romPaths.rompath) + CFG_EXT;
		saveButtonsConfig(rutaGuardado, false);
		return rutaGuardado;
	} else {
		return "";
	}
}

std::string Joystick::saveButtonsDefaultsCore() {
	if (!CfgLoader::coreDefault.empty()){
		std::string coreDefaultsPath = Constant::getAppDir() + std::string(Constant::tempFileSep) + "config"
			+ std::string(Constant::tempFileSep) + PREFIX_DEFAULTS + CfgLoader::configMain[cfg::libretro_core].valueStr + CFG_EXT;

		saveButtonsConfig(coreDefaultsPath, false);
		return coreDefaultsPath;
	} else {
		return "";
	}
}
/**
*
*/
std::string Joystick::saveButtonsRetroCore() {
	std::string rutaGuardado = Constant::getAppDir() + Constant::getFileSep() + RETROPAD_INI;
	return saveButtonsConfig(rutaGuardado);
}

/**
*
*/
std::string Joystick::saveButtonsConfig(std::string ruta, bool hotkeysAndFrontend) {
    std::vector<std::string> fileConfigJoystick;
    fileConfigJoystick.push_back("[RETROPAD_LIST]");

    // Vector para guardar el nombre del perfil asignado a cada jugador
    std::vector<std::string> playerProfileNames(MAX_PLAYERS, "");
    // Para rastrear firmas de configuración ya escritas y evitar duplicados
    std::vector<std::string> savedSignatures;

    for (int p = 0; p < MAX_PLAYERS; p++) {
        if (inputs.names[p].empty()) {
            playerProfileNames[p] = "None";
            continue;
        }

        // 1. Generar una "firma" única del mapeo de este jugador
        std::string signature = "";
        for (int i = 0; i < MAX_BUTTONS; i++) signature += Constant::intToString(inputs.mapperCore.sdlToBtn[p][i]) + ",";
        signature += "|";
        for (int i = 0; i < MAX_HATS; i++)    signature += Constant::intToString(inputs.mapperCore.sdlToHat[p][i]) + ",";
        signature += "|";
        for (int i = 0; i < MAX_AXIS; i++)    signature += Constant::intToString(inputs.mapperCore.sdlToAxis[p][i]) + ",";
        signature += (inputs.axisAsPad[p] ? "1" : "0");

        // 2. Verificar si esta configuración exacta ya fue guardada
        bool yaEscrito = false;
        for (std::size_t s = 0; s < savedSignatures.size(); s++) {
            if (savedSignatures[s] == signature) {
                // Buscamos que nombre le pusimos a esa firma anteriormente
                for (int prev = 0; prev < p; prev++) {
                    // Si encontramos al jugador previo con la misma firma, copiamos su nombre de perfil
                    playerProfileNames[p] = playerProfileNames[prev];
                    yaEscrito = true;
                    break;
                }
                break;

			}
        }

        // 3. Si es una configuracion nueva o el nombre base ya existe, generar perfil
        if (!yaEscrito) {
            std::string baseName = Constant::Trim(inputs.names[p]);
            std::string finalMapperName = baseName;
            
            // Evitar colision de nombres de perfiles en el INI
            int count = 0;
            for (int i = 0; i < p; i++) {
                if (playerProfileNames[i] == finalMapperName) {
                    count++;
                    finalMapperName = baseName + "(" + Constant::intToString(count) + ")";
                    i = -1; // Reiniciar check para el nuevo nombre
                }
            }

            playerProfileNames[p] = finalMapperName;
            savedSignatures.push_back(signature);

            // Escribir bloque de configuracion
            fileConfigJoystick.push_back("name=" + finalMapperName);
            
            std::string btns = "btns=";
            for (int i = 0; i < MAX_BUTTONS; i++) 
                btns += Constant::intToString(inputs.mapperCore.sdlToBtn[p][i]) + (i < MAX_BUTTONS - 1 ? "," : "");
            fileConfigJoystick.push_back(btns);

            std::string hats = "hats=";
            for (int i = 0; i < MAX_HATS; i++) 
                hats += Constant::intToString(inputs.mapperCore.sdlToHat[p][i]) + (i < MAX_HATS - 1 ? "," : "");
            fileConfigJoystick.push_back(hats);

            std::string axis = "axis=";
            for (int i = 0; i < MAX_AXIS; i++) 
                axis += Constant::intToString(inputs.mapperCore.sdlToAxis[p][i]) + (i < MAX_AXIS - 1 ? "," : "");
            fileConfigJoystick.push_back(axis);

            fileConfigJoystick.push_back("anal=" + std::string(inputs.axisAsPad[p] ? "1" : "0"));
            fileConfigJoystick.push_back(""); // Linea en blanco
        }
    }

    // 4. Seccion de asignacion por jugador
    fileConfigJoystick.push_back("[RETROPAD]");
    for (int p = 0; p < MAX_PLAYERS; p++) {
        fileConfigJoystick.push_back("player" + Constant::intToString(p) + "_name=" + playerProfileNames[p]);
    }

	if (hotkeysAndFrontend){
		fileConfigJoystick.push_back(""); // Linea en blanco
		fileConfigJoystick.push_back("[HOTKEYS]");
		std::string btns = "btns=";
		for (int i = 0; i < MAX_BUTTONS; i++) 
			btns += Constant::intToString(inputs.mapperHotkeys.sdlToBtn[0][i]) + (i < MAX_BUTTONS - 1 ? "," : "");
		fileConfigJoystick.push_back(btns);

		std::string hats = "hats=";
		for (int i = 0; i < MAX_HATS; i++) 
			hats += Constant::intToString(inputs.mapperHotkeys.sdlToHat[0][i]) + (i < MAX_HATS - 1 ? "," : "");
		fileConfigJoystick.push_back(hats);

		std::string axis = "axis=";
		for (int i = 0; i < MAX_AXIS; i++) 
			axis += Constant::intToString(inputs.mapperHotkeys.sdlToAxis[0][i]) + (i < MAX_AXIS - 1 ? "," : "");
		fileConfigJoystick.push_back(axis);

		fileConfigJoystick.push_back(""); // Linea en blanco
		fileConfigJoystick.push_back("[FRONTEND]");
		btns = "btns=";
		for (int i = 0; i < MAX_BUTTONS; i++) 
			btns += Constant::intToString(inputs.mapperFrontend.sdlToBtn[0][i]) + (i < MAX_BUTTONS - 1 ? "," : "");
		fileConfigJoystick.push_back(btns);

		hats = "hats=";
		for (int i = 0; i < MAX_HATS; i++) 
			hats += Constant::intToString(inputs.mapperFrontend.sdlToHat[0][i]) + (i < MAX_HATS - 1 ? "," : "");
		fileConfigJoystick.push_back(hats);

		axis = "axis=";
		for (int i = 0; i < MAX_AXIS; i++) 
			axis += Constant::intToString(inputs.mapperFrontend.sdlToAxis[0][i]) + (i < MAX_AXIS - 1 ? "," : "");
		fileConfigJoystick.push_back(axis);
	}
    FileList::guardarVector(ruta, fileConfigJoystick);
    return ruta;
}

/**
*
*/
bool Joystick::loadButtonsRetro(std::string ruta) {
    
    std::vector<std::string> lineas;
	FileList::cargarVector(ruta, lineas);
    if (lineas.empty()) return false;

    // Estructura temporal para almacenar los perfiles definidos en RETROPAD_LIST
    struct PadProfile {
        std::vector<int> btns, hats, axis;
        bool anal;
    };
    std::map<std::string, PadProfile> profiles;
    std::string currentSection = "";
    std::string currentProfileName = "";

    for (std::size_t i = 0; i < lineas.size(); ++i) {
        std::string line = Constant::Trim(lineas[i]);
        if (line.empty()) continue;

        // Cambio de secci�n
        if (line[0] == '[' && line[line.size() - 1] == ']') {
            currentSection = line;
            continue;
        }

        // --- PARSEO DE SECCIONES ---
        if (currentSection == "[RETROPAD_LIST]") {
            if (line.find("name=") == 0) {
                currentProfileName = line.substr(5);
            } else if (!currentProfileName.empty()) {
                if (line.find("btns=") == 0) 
                    profiles[currentProfileName].btns = Constant::splitInt(line.substr(5), ',');
                else if (line.find("hats=") == 0)
                    profiles[currentProfileName].hats = Constant::splitInt(line.substr(5), ',');
                else if (line.find("axis=") == 0)
                    profiles[currentProfileName].axis = Constant::splitInt(line.substr(5), ',');
                else if (line.find("anal=") == 0)
                    profiles[currentProfileName].anal = (line.substr(5) == "1");
            }
        } 
        else if (currentSection == "[RETROPAD]") {
            // Formato: player0_name=Xbox Controller
            std::size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string key = line.substr(0, eqPos);
                std::string profileName = line.substr(eqPos + 1);
                
                // Extraer el numero de jugador de "playerX_name"
				int p = Constant::strToTipo<int>(key.substr(6, key.find('_') - 6));
                
                if (p < MAX_PLAYERS && profiles.count(profileName)) {
                    PadProfile& pf = profiles[profileName];
                    for (std::size_t j = 0; j < MAX_BUTTONS && j < pf.btns.size(); j++) inputs.mapperCore.setBtnFromSdl(p, j, pf.btns[j]);
                    for (std::size_t j = 0; j < MAX_HATS && j < pf.hats.size(); j++)    inputs.mapperCore.setHatFromSdl(p, j, pf.hats[j]);
                    for (std::size_t j = 0; j < MAX_AXIS && j < pf.axis.size(); j++)    inputs.mapperCore.setAxisFromSdl(p, j, pf.axis[j]);
                    inputs.axisAsPad[p] = pf.anal;
					inputs.names[p] = profileName;
                }
            }
        }
        else if (currentSection == "[HOTKEYS]" || currentSection == "[FRONTEND]") {
            auto& targetMapper = (currentSection == "[HOTKEYS]") ? inputs.mapperHotkeys : inputs.mapperFrontend;
            if (line.find("btns=") == 0) {
                std::vector<int> v = Constant::splitInt(line.substr(5), ',');
                for (std::size_t j = 0; j < MAX_BUTTONS && j < v.size(); j++) targetMapper.setBtnFromSdl(0, j, v[j]);
            } else if (line.find("hats=") == 0) {
                std::vector<int> v = Constant::splitInt(line.substr(5), ',');
                for (std::size_t j = 0; j < MAX_HATS && j < v.size(); j++) targetMapper.setHatFromSdl(0, j, v[j]);
            } else if (line.find("axis=") == 0) {
                std::vector<int> v = Constant::splitInt(line.substr(5), ',');
                for (std::size_t j = 0; j < MAX_AXIS && j < v.size(); j++) targetMapper.setAxisFromSdl(0, j, v[j]);
            }
        }
    }
    return true;
}

/**
*
*/
void Joystick::close_joysticks() {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (g_joysticks[i]) {
			SDL_JoystickClose(g_joysticks[i]);
		}
		g_joysticks[i] = NULL;
	}
}

/**
*
*/
bool Joystick::pollKeys(SDL_Surface* screen){
    SDL_Event event;

	#ifdef _XBOX
    // Optimización: Solo iterar si hay algún frame pendiente de liberar
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (startHoldFrames[i] > 0) {
            if (--startHoldFrames[i] == 0) {
                int sdlBtn = inputs.mapperCore.getSdlBtn(i, RETRO_DEVICE_ID_JOYPAD_START);
                if ((unsigned int)sdlBtn < MAX_BUTTONS) {
                    inputs.btn_state[i][sdlBtn] = false;
                }
            }
        }
    }
    #endif

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                evento.quit = true;
                break;

            case SDL_JOYBUTTONUP:
            case SDL_JOYBUTTONDOWN: {
				const unsigned int p = (unsigned int)event.jbutton.which;
				if (p >= MAX_PLAYERS) break;
                const unsigned int btn = (unsigned int)event.jbutton.button;
                if (btn < MAX_BUTTONS) {
                    bool isDown = (event.type == SDL_JOYBUTTONDOWN);
                    inputs.btn_state[p][btn] = isDown;
                    
                    #ifdef _XBOX
                    // Solo entramos si el botón pulsado es el mapeado como START
                    if (isDown && btn == (unsigned int)inputs.mapperCore.getSdlBtn(p, RETRO_DEVICE_ID_JOYPAD_START)) {
                        startHoldFrames[p] = 3;
                    }
                    #endif
                }
                break;
            }

            case SDL_JOYHATMOTION: {
	            const unsigned int p = (unsigned int)event.jhat.which;
				if (p >= MAX_PLAYERS) break;
                const Uint8 val = event.jhat.value;
                // Branchless: convertimos los bits del hat directamente a bool
                bool* hState = inputs.hats_state[p];
                hState[SDL_HAT_UP]    = (val & SDL_HAT_UP) != 0;
                hState[SDL_HAT_DOWN]  = (val & SDL_HAT_DOWN) != 0;
                hState[SDL_HAT_LEFT]  = (val & SDL_HAT_LEFT) != 0;
                hState[SDL_HAT_RIGHT] = (val & SDL_HAT_RIGHT) != 0;
                break;
            }

            case SDL_JOYAXISMOTION: {
				// Usamos unsigned para evitar chequeos de < 0 y optimizar comparaciones
				const unsigned int p = (unsigned int)event.jaxis.which;
				if (p >= MAX_PLAYERS) break;

                const unsigned int axis = (unsigned int)event.jaxis.axis;
                if (axis >= MAX_AXIS) break;
				bool combinedAxis = false;
				#ifndef _XBOX
				//En xbox los gatillos L2 y R2 no son ejes. Se comportan como botones, al menos en la 
				//libreria de xbox 360 de Lantus, por lo que no hace falta hacer esto para forzar a 
				//que los gatillos se comporten como botones siempre
				combinedAxis = (axis == XBOX_COMBINED_TRIGGER_AXIS); 
				#endif

                if (inputs.axisAsPad[p] || combinedAxis) {
                    // Pre-calculamos los índices para evitar multiplicar por 2 varias veces
                    const int idxNeg = axis << 1;      // axis * 2
                    const int idxPos = idxNeg | 1;     // axis * 2 + 1
                    const Sint16 val = event.jaxis.value;
                    bool* axisState = inputs.axis_state[p];

                    // Usamos una lógica más plana para el compilador
                    axisState[idxPos] = (val >  DEADZONE);
                    axisState[idxNeg] = (val < -DEADZONE);
                } else {
                    int32_t raw = event.jaxis.value;
                    // Clampeo seguro: evitamos el overflow de 32767
                    if (raw >  32760) raw =  32760;
                    if (raw < -32760) raw = -32760;
                    inputs.g_analog_state[p][axis] = (int16_t)raw;
					//LOG_INFO("axis: %d=%d", axis, inputs.g_analog_state[p][axis]);
                }
                break;
            }
        }
    }
    return true;
}


/**
*
*/
HOTKEYS_LIST Joystick::findHotkey(){
	return hotkeys->procesarHotkeys(&inputs);
	return HK_MAX;
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