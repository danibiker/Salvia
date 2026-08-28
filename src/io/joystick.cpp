#include <SDL.h>
#include <io/joystick.h>
#include <io/hotkeys.h>
#include <io/filelist.h>
#include <io/dirutil.h>
#include <const/constant.h>
#include <io/cfgloader.h>
#include <io/fileio.h>
#include <cmath>
#include <libretro.h>

extern void retro_set_controller_port_device(unsigned port, unsigned device);

// Bridge to the core's libretro keyboard callback (registered via
// RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK in salvia.cpp). DOSBox-Pure
// and similar cores depend on receiving key events through this path.
extern "C" void salvia_dispatch_keyboard_event(bool down, unsigned retro_keycode,
                                               uint32_t character, uint16_t modifiers);

extern retro_keyboard_event_t core_key_callback;

// Estructura temporal para almacenar los perfiles definidos en RETROPAD_LIST
struct PadProfile {
    std::vector<int> btns, hats, axis;
    bool anal;
	int joyTypeIdx;
	PadProfile(): anal(false), joyTypeIdx(0) {}
};

Joystick::Joystick(){
	ignoreButtonRepeats = false;
	this->w = 0;
    this->h = 0;
    gestorCursor = new CursorGestor();
	setCursor(cursor_hidden);
	hotkeys = new Hotkeys(&this->inputs);
	memset(startHoldFrames, 0, sizeof(startHoldFrames));
	infoButtonsDirty = true;
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
			/* axisAsPad must be false by default to allow analog-joystick-able cores 
			 * as dosbox-pure or tyrquake to use the input properly */
			inputs.axisAsPad[joyId] = false;
			inputs.joyTypeIdx[joyId] = 0;
			//Setting mappers for the frontend
			configMapperFrontend(inputs.mapperFrontend, joyId);
			//Setting mappers for the core's emulator
			configMapperRetro(inputs.mapperCore, joyId);
		}
	}
	std::string ruta = Constant::getAppDir() + Constant::getFileSep() + RETROPAD_INI;
	loadButtonsRetro(ruta);
	return true;
}

void Joystick::updateTypes(){
	for (int i=0; i < MAX_PLAYERS; i++){
		if (inputs.joyTypeIdx[i] < (int)g_ports[i].available_types.size()){
			auto joyType = g_ports[i].available_types[inputs.joyTypeIdx[i]];
			g_ports[i].current_device_id = joyType.first;
			g_ports[i].current_desc = joyType.second;
		} else {
			g_ports[i].current_device_id = RETRO_DEVICE_JOYPAD;
			g_ports[i].current_desc = "Retro pad";
		}
		retro_set_controller_port_device(i, g_ports[i].current_device_id);
	}
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
		std::string rutaGuardado = dir.getFolder(romPaths.rompath) + Constant::getFileSep() + dir.getFileNameNoExt(romPaths.rompath) + CFG_JOY_EXT;
		saveButtonsConfig(rutaGuardado, false);
		return rutaGuardado;
	} else {
		return "";
	}
}

std::string Joystick::saveButtonsRetroCore() {
	std::string coreDefaultsPath = Constant::getAppDir() + std::string(Constant::tempFileSep) + "config"
		+ std::string(Constant::tempFileSep) + PREFIX_DEFAULTS + CfgLoader::configMain[cfg::libretro_core].valueStr + CFG_JOY_EXT;

	saveButtonsConfig(coreDefaultsPath, false);
	return coreDefaultsPath;
}
/**
*
*/
std::string Joystick::saveButtonsRetroDefault() {
	std::string rutaGuardado = Constant::getAppDir() + Constant::getFileSep() + RETROPAD_INI;
	setInfoButtons();
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
		
		if (!hotkeysAndFrontend)
			signature += inputs.joyTypeIdx[p];

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

			//Si estamos guardando la configuracion general, no tenemos en cuenta el tipo de joystick
			//ni el analogico porque es independiente del core elegido
			if (!hotkeysAndFrontend){
				int joyType = inputs.joyTypeIdx[p];
				int anal = inputs.axisAsPad[p] ? 1 : 0;
				fileConfigJoystick.push_back("anal=" + Constant::TipoToStr(anal));
				fileConfigJoystick.push_back("joytype=" + Constant::TipoToStr(joyType));
			}
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

		std::string anal = "anal=";
		anal += Constant::intToString(inputs.frontAxisAsPad);
		fileConfigJoystick.push_back(anal);
	}
    FileList::guardarVector(ruta, fileConfigJoystick);
	Fileio::commit(ruta.c_str());
    return ruta;
}

/**
*
*/
bool Joystick::loadButtonsRetro(std::string ruta) {
    
    std::vector<std::string> lineas;
    std::map<std::string, PadProfile> profiles;
    std::string currentSection = "";
    std::string currentProfileName = "";
	int foundProfiles = 0;
	bool foundFirstJoy = false;

	FileList::cargarVector(ruta, lineas);
    if (lineas.empty()) return false;

    for (std::size_t i = 0; i < lineas.size(); ++i) {
        std::string line = Constant::Trim(lineas[i]);
        if (line.empty()) continue;

        // Cambio de seccion
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
				else if (line.find("joytype=") == 0)
					profiles[currentProfileName].joyTypeIdx = Constant::strToTipo<int>(line.substr(8));
            }
        } else if (currentSection == "[RETROPAD]") {
            // Formato: player0_name=Xbox Controller
            std::size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string key = line.substr(0, eqPos);
                std::string profileName = line.substr(eqPos + 1);
                
                // Incrementamos siempre de antemano. Si entra en el 'if' del primer mando, se sobrescribira.
				foundProfiles++;
				int p = foundProfiles;

				if (!foundFirstJoy) {
					// Comparamos el nombre del perfil con el mando conectado al primer puerto
					if (profileName.find(inputs.names[0]) != std::string::npos) {
						p = 0;
						foundFirstJoy = true;
						// Compensamos el incremento previo ya que este caso no consume un perfil generico
						foundProfiles--; 
					}
				}

                if (p < MAX_PLAYERS && profiles.count(profileName)) {
                    PadProfile& pf = profiles[profileName];
                    for (std::size_t j = 0; j < MAX_BUTTONS && j < pf.btns.size(); j++) inputs.mapperCore.setBtnFromSdl(p, j, pf.btns[j]);
                    for (std::size_t j = 0; j < MAX_HATS && j < pf.hats.size(); j++)    inputs.mapperCore.setHatFromSdl(p, j, pf.hats[j]);
                    for (std::size_t j = 0; j < MAX_AXIS && j < pf.axis.size(); j++)    inputs.mapperCore.setAxisFromSdl(p, j, pf.axis[j]);
                    inputs.axisAsPad[p] = pf.anal;
					inputs.names[p] = profileName;
					//First assign the joystick type...
					inputs.joyTypeIdx[p] = pf.joyTypeIdx;
					//... and afterwards, make sure is not RETRO_DEVICE_NONE if there are more options
					//this is needed for dosbox-pure, to assign the default option to a joystick controller
					getCkeckedJoyTypeIndex(p);
                }
            }
        } else if (currentSection == "[HOTKEYS]" || currentSection == "[FRONTEND]") {
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
            } else if (line.find("anal=") == 0){
				//Set a proper option for the frontend and don't rely only with the option assigned to the core
				inputs.frontAxisAsPad = (line.substr(5) == "1");
			}
        }
    }
    return true;
}

/**
* get the joytype index and make sure that the one we select is not RETRO_DEVICE_NONE if we
* have other options. Choose always RETRO_DEVICE_JOYPAD, if there was no device set
*/
int Joystick::getCkeckedJoyTypeIndex(int jPos){
	if (jPos < 0 || jPos >= MAX_PLAYERS)
		return 0;

	t_controller_port *port = &g_ports[jPos];

	if (port->available_types.size() > 0){
		// [XBOX360] Default sensato: NO quedarnos en "Disabled"
		// (RETRO_DEVICE_NONE). joyTypeIdx (indice en available_types) es lo
		// que updateTypes() aplica al core via retro_set_controller_port_device.
		// Solo lo ajustamos si apunta a NONE o esta fuera de rango (no pisamos
		// una eleccion valida ya hecha por el usuario/perfil). Preferimos
		// RETRO_DEVICE_JOYPAD: en dosbox-pure es "Use Gamepad Mapper" (el Pad
		// Mapper); en pcsxr-360 es "standard". Si no hay JOYPAD, el primer tipo
		// que no sea Disabled.
		int curIdx = inputs.joyTypeIdx[jPos];
		bool needDefault = (curIdx < 0
			|| curIdx >= (int)port->available_types.size()
			|| port->available_types[curIdx].first == RETRO_DEVICE_NONE);

		LOG_DEBUG("needDefault %u...", needDefault ? 1 : 0);

		if (needDefault) {
			int chosen = -1, firstNonNone = -1;
			for (std::size_t k = 0; k < port->available_types.size(); ++k) {
				unsigned tid = port->available_types[k].first;
				if (tid == RETRO_DEVICE_JOYPAD) { chosen = (int)k; break; }
				if (firstNonNone < 0 && tid != RETRO_DEVICE_NONE) 
					firstNonNone = (int)k;
			}
			if (chosen < 0) chosen = (firstNonNone >= 0) ? firstNonNone : 0;
			LOG_DEBUG("chosen %d...", chosen);
			inputs.joyTypeIdx[jPos] = chosen;
		}
		port->current_device_id = port->available_types[inputs.joyTypeIdx[jPos]].first;
		LOG_DEBUG("port->current_device_id %d...", port->current_device_id);
	}
	return inputs.joyTypeIdx[jPos];
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
bool Joystick::pollKeys(int gameStatus){
    SDL_Event event;

	inputs.mouse_rel_x = 0;
	inputs.mouse_rel_y = 0;

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
                /* La rama de "eje como pad" escribe en axis_state[p][axis*2+1],
                 * asi que el limite util es MAX_AXIS/2, no MAX_AXIS: con axis
                 * >= 16 se escribia fuera del array. Con mandos reales (<= 6
                 * ejes) no llegaba a pasar, pero la guarda estaba mal.
                 * g_analog_state, la otra rama, se indexa con axis a secas y
                 * tiene MAX_ANALOG_AXIS entradas. */
                if (axis >= MAX_AXIS / 2 || axis >= MAX_ANALOG_AXIS) break;
				bool combinedAxis = false;
				#ifndef _XBOX
				//En xbox los gatillos L2 y R2 no son ejes. Se comportan como botones, al menos en la 
				//libreria de xbox 360 de Lantus, por lo que no hace falta hacer esto para forzar a 
				//que los gatillos se comporten como botones siempre
				combinedAxis = (axis == XBOX_COMBINED_TRIGGER_AXIS); 
				#endif

				const bool isPadInput = gameStatus == EMU_STARTED ? (inputs.axisAsPad[p] || combinedAxis) : inputs.frontAxisAsPad;

				if (isPadInput) {
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
                    // [XBOX360] Clamp SIMETRICO a +-32767 (estandar libretro/RetroArch). El unico
                    // valor problematico es INT16_MIN (-32768): al golpear el stick a fondo
                    // (izquierda rapida) SDL entrega -32768, y los cores que NIEGAN el eje
                    // para mapeos invertidos (dosbox-pure, meta=-1) calculan -(-32768), que
                    // NO cabe en int16 -> se queda -32768 y el input "se desactiva" en esa
                    // direccion. El positivo ya llega como maximo a +32767. Rango completo
                    // simetrico (lo que espera pcsxr-360; su "andar vs correr" era del core).
					if (raw < -32767) raw = -32767;
                    inputs.g_analog_state[p][axis] = (int16_t)raw;
                }
                break;
            }

			case SDL_MOUSEMOTION: {
			// Coordenadas absolutas
				inputs.mouse_x = event.motion.x;
				inputs.mouse_y = event.motion.y;
				// Movimiento relativo (útil para shooters o juegos que capturan el cursor)
				inputs.mouse_rel_x += event.motion.xrel;
				inputs.mouse_rel_y += event.motion.yrel;
				break;
			}

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP: {
				bool isDown = (event.type == SDL_MOUSEBUTTONDOWN);
				switch (event.button.button) {
				case SDL_BUTTON_LEFT:   inputs.mouse_buttons[0] = isDown; break;
				case SDL_BUTTON_MIDDLE: inputs.mouse_buttons[1] = isDown; break;
				case SDL_BUTTON_RIGHT:  inputs.mouse_buttons[2] = isDown; break;
				}
				break;
			} 

			/*case SDL_MOUSEWHEEL: {
				// event.wheel.y es positivo para arriba, negativo para abajo
				inputs.mouse_wheel = event.wheel.y; 
				break;
			}*/

			case SDL_KEYDOWN: {
				// MapSDLKeyToLibRetro hace bounds-check; sym >= SDLK_LAST → RETROK_UNKNOWN.
				uint16_t retro_key = MapSDLKeyToLibRetro(event.key.keysym.sym);
				uint16_t retro_mod = MapSDLModToLibRetro(event.key.keysym.mod);
				// `character` para el callback: SDL ya traduce Shift/CapsLock al
				// campo `unicode` cuando SDL_EnableUNICODE(1) funciona. En ports
				// donde no funciona (Xbox 360 SDL) llega 0 — usamos fallback
				// manual para letras y dígitos US-QWERTY.
				uint32_t character = event.key.keysym.unicode;
				if (character == 0) {
					character = SDLKeyToASCIIFallback(event.key.keysym.sym,
					                                   event.key.keysym.mod);
				}

				if (core_key_callback) {
					// Despachar SIEMPRE al callback del core, incluso si la tecla no
					// está en nuestra tabla — algunos cores hacen su propio mapeo.
					salvia_dispatch_keyboard_event(true, retro_key, character, retro_mod);
				} else if (retro_key < t_joy_state::MAX_RETRO_KEYS && retro_key != RETROK_UNKNOWN) {
					t_key_input *keyInput = &inputs.keyboard_state[retro_key];
					keyInput->keyjoydown = true;
					keyInput->key        = retro_key;
					keyInput->keyMod     = retro_mod;
					keyInput->unicode    = character;
				}
				
				break;
			}

			case SDL_KEYUP: {
				uint16_t retro_key = MapSDLKeyToLibRetro(event.key.keysym.sym);
				uint16_t retro_mod = MapSDLModToLibRetro(event.key.keysym.mod);
				uint32_t character = event.key.keysym.unicode;
				if (character == 0) {
					character = SDLKeyToASCIIFallback(event.key.keysym.sym,
					                                   event.key.keysym.mod);
				}
				if (core_key_callback) {
					salvia_dispatch_keyboard_event(false, retro_key, character, retro_mod);
				} else if (retro_key < t_joy_state::MAX_RETRO_KEYS && retro_key != RETROK_UNKNOWN) {
					inputs.keyboard_state[retro_key].keyjoydown = false;
				}

				inputs.last_key_processed->key = retro_key;
				inputs.last_key_processed->keyMod = retro_mod;
				inputs.last_key_processed->unicode = character;
				inputs.last_key_processed->keyjoydown = false;

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

void Joystick::setInfoButtons(){
	//Dando valor a la ayuda de los botones que se muestra en el menu
	const int num_port_buttons = sizeof(FRONTEND_BTN_VAL) / sizeof(FRONTEND_BTN_VAL[0]);
	//Son las posiciones de los botones que nos interesan en FRONTEND_BTN_VAL y FRONTEND_BTN_TXT
	const int buttonsToShowInfo[] = {4, 5, 7, 8, 9, 6};
	const bool mergeNextArr[] = {false, false, true, false, false, false};
	const int num_port_buttons_info = sizeof(buttonsToShowInfo) / sizeof(buttonsToShowInfo[0]);

	infoButtons.clear();	
	for (int i=0; i < num_port_buttons_info; i++){
		t_info_btn btn;
		btn.description = FRONTEND_BTN_TXT[buttonsToShowInfo[i]];
		const int sdlIdBtn = inputs.mapperFrontend.getSdlBtn(0, FRONTEND_BTN_VAL[buttonsToShowInfo[i]]);

		/* sdlIdBtn es el numero de boton que da el mando: acotarlo antes de
		 * indexar la tabla de etiquetas, que solo tiene SDL_BTN_TO_XBOX_SIZE. */
		if (sdlIdBtn < 0 || sdlIdBtn >= SDL_BTN_TO_XBOX_SIZE)
			continue;

		btn.text = std::string(SDL_BTN_TO_XBOX[sdlIdBtn]);
		btn.mergeNext = mergeNextArr[i];
		
		if (btn.text == "R3" || btn.text == "L3")
			btn.shape = BS_DOUBLE_CIRCLE;
		else 
			btn.shape = BS_CIRCLE;

		infoButtons.push_back(btn);
		LOG_DEBUG("Added  key info: %s -> %s", btn.description.c_str(), btn.text.c_str());
	}
	infoButtonsDirty = true;
}