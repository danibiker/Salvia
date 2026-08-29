#include <SDL.h>
#include <io/joystick.h>
#include <io/hotkeys.h>
#include <io/filelist.h>
#include <io/dirutil.h>
#include <const/constant.h>
#include <io/cfgloader.h>
#include <io/fileio.h>
#include <cmath>
#include <set>
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
    std::vector<int> anlg;
	int joyTypeIdx;
	/* 'anlg=' es una clave posterior al resto del formato. Sin este flag, un .joy
	 * antiguo (que no la trae) aplicaria el vector vacio y machacaria los defaults
	 * que dejo configMapperRetro, igual que hacen hoy anal= y joytype=. */
	bool hasAnlg;
	PadProfile(): joyTypeIdx(0), hasAnlg(false) {}
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
			/* Copia que no pisa la carga: es la que permite reconocer el mando. */
			inputs.physicalNames[joyId] = inputs.names[joyId];
			/* Cuantos ejes/botones/hats dice SDL que tiene el mando. Sin esto no hay
			 * forma de saber que numero de eje es cada stick, que NO coincide entre
			 * plataformas ni entre drivers, y es de lo primero que hace falta cuando
			 * un remapeo no responde. */
			LOG_INFO("Joystick %d: '%s' -> %d ejes, %d botones, %d hats", joyId,
				inputs.names[joyId].c_str(),
				SDL_JoystickNumAxes(g_joysticks[joyId]),
				SDL_JoystickNumButtons(g_joysticks[joyId]),
				SDL_JoystickNumHats(g_joysticks[joyId]));
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

	/* Direcciones analogicas: por defecto IDENTIDAD, o sea cada direccion del stick
	 * se comporta como ella misma y el core recibe el analogico de siempre. El que
	 * quiera el stick como cruceta asigna sus cuatro direcciones a posiciones de
	 * hat; ya no hay interruptor global (el antiguo axisAsPad).
	 * Solo se asignan las direcciones que el mando puede dar: naxis es el numero de
	 * direcciones fisicas disponibles (ejes * 2). Las que no lleguen quedan a -1. */
	for (int slot = 0; slot < ANALOG_TARGETS; slot++){
		const int virt = analogSlotAxis[slot];
		mapper.setAnalogDst(joyId, slot, virt < naxis ? virt : -1);
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
* Lee los perfiles que YA hay en el fichero que vamos a sobrescribir.
*
* Hace falta porque saveButtonsConfig reescribe el fichero entero a partir de los
* mandos CONECTADOS en ese momento: sin esto, guardar con un mando de Xbox borraba
* el perfil que se hubiera guardado antes con, por ejemplo, uno de SNES. El formato
* siempre admitio varios bloques 'name=' en [RETROPAD_LIST] -- lo que faltaba era
* conservarlos.
*
* Los bloques se guardan como TEXTO CRUDO a proposito: de un perfil de un mando que
* no esta conectado no queremos reinterpretar nada, solo devolverlo intacto (incluidas
* claves que esta version no conozca).
*
* 'orden' preserva el orden de aparicion para que el fichero no se baraje en cada
* guardado y los diffs sigan siendo legibles.
*/
static void readExistingProfiles(const std::string &ruta,
                                 std::vector<std::string> &orden,
                                 std::map<std::string, std::vector<std::string> > &bloques,
                                 std::map<int, std::string> &asignaciones) {
    std::vector<std::string> lineas;
    std::string currentSection = "";
    std::string currentProfile = "";

    FileList::cargarVector(ruta, lineas);

    for (std::size_t i = 0; i < lineas.size(); ++i) {
        std::string line = Constant::Trim(lineas[i]);
        if (line.empty()) continue;

        if (line[0] == '[' && line[line.size() - 1] == ']') {
            currentSection = line;
            currentProfile = "";
            continue;
        }

        if (currentSection == "[RETROPAD_LIST]") {
            if (line.find("name=") == 0) {
                currentProfile = line.substr(5);
                if (!bloques.count(currentProfile)) orden.push_back(currentProfile);
                bloques[currentProfile].clear();
            } else if (!currentProfile.empty()) {
                bloques[currentProfile].push_back(line);
            }
        } else if (currentSection == "[RETROPAD]") {
            std::size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;
            std::string key = line.substr(0, eqPos);
            int p = -1;
            for (std::size_t c = 0; c < key.size(); c++) {
                if (key[c] >= '0' && key[c] <= '9') { p = key[c] - '0'; break; }
            }
            if (p >= 0 && p < MAX_PLAYERS) asignaciones[p] = line.substr(eqPos + 1);
        }
    }
}

/**
*
*/
std::string Joystick::saveButtonsConfig(std::string ruta, bool hotkeysAndFrontend) {
    std::vector<std::string> fileConfigJoystick;

    /* Lo que ya hubiera en el fichero, para fusionarlo en vez de machacarlo. */
    std::vector<std::string> prevOrden;
    std::map<std::string, std::vector<std::string> > prevBloques;
    std::map<int, std::string> prevAsignacion;
    readExistingProfiles(ruta, prevOrden, prevBloques, prevAsignacion);

    /* Perfiles que escribe ESTA pasada. Un perfil previo con el mismo nombre queda
     * sustituido por el nuevo, que es justo lo que se espera al reguardar el mismo
     * mando; los demas se copian tal cual mas abajo. */
    std::set<std::string> perfilesEscritos;

    fileConfigJoystick.push_back("[RETROPAD_LIST]");

    // Vector para guardar el nombre del perfil asignado a cada jugador
    std::vector<std::string> playerProfileNames(MAX_PLAYERS, "");
    /* Firma de mapeo -> nombre del perfil que ya la escribio. Era un vector que solo
     * guardaba la firma, asi que al encontrar una repetida no habia forma de saber DE
     * QUE JUGADOR era, y el codigo de abajo copiaba el nombre del jugador 0 sin
     * comprobar nada. */
    std::map<std::string, std::string> savedSignatures;

    /* Un perfil pertenece a un MANDO, asi que tanto la condicion de "hay algo que
     * guardar" como el nombre del bloque salen de physicalNames, no de names.
     *
     * Con names[] el resultado era erroneo en cuanto habia mas de un perfil en el
     * fichero: la carga deja en names[p] el nombre del perfil aplicado, que puede ser
     * el de OTRO mando (cuando se resuelve por el respaldo player<N>_name), y al
     * guardar se habria sobrescrito el perfil de ese otro mando con este mapeo. */
    for (int p = 0; p < MAX_PLAYERS; p++) {
        if (inputs.physicalNames[p].empty()) {
            playerProfileNames[p] = "None";
            continue;
        }

        // 1. Generar una "firma" única del mapeo de este jugador
        std::string signature = "";
        for (int i = 0; i < MAX_SDL_BUTTONS; i++)    signature += Constant::intToString(inputs.mapperCore.sdlToBtn[p][i]) + ",";
        signature += "|";
        for (int i = 0; i < MAX_SDL_HAT_VALUES; i++) signature += Constant::intToString(inputs.mapperCore.sdlToHat[p][i]) + ",";
        signature += "|";
        for (int i = 0; i < MAX_SDL_AXIS_DIRS; i++)  signature += Constant::intToString(inputs.mapperCore.sdlToAxis[p][i]) + ",";
        signature += "|";
        for (int i = 0; i < ANALOG_TARGETS; i++) signature += Constant::intToString(inputs.mapperCore.analogDst[p][i]) + ",";

		if (!hotkeysAndFrontend)
			/* Era 'signature += inputs.joyTypeIdx[p]', o sea operator+=(char): metia
			 * el CARACTER con ese codigo, no los digitos. Con joyTypeIdx==0 colaba un
			 * '\0' en medio de la firma y la deduplicacion no distinguia tipos. */
			signature += Constant::intToString(inputs.joyTypeIdx[p]);

        /* 2. Si esta configuracion exacta ya se escribio, reusar SU nombre de perfil.
         * El bucle anterior recorria los jugadores previos pero asignaba y hacia break
         * en la primera iteracion, sin comparar: siempre copiaba el nombre del jugador
         * 0. Con el jugador 0 en "None" y los jugadores 1 y 2 compartiendo mapeo, el 2
         * acababa con "player2_name=None". */
        bool yaEscrito = false;
        std::map<std::string, std::string>::iterator itSig = savedSignatures.find(signature);
        if (itSig != savedSignatures.end()) {
            playerProfileNames[p] = itSig->second;
            yaEscrito = true;
        }

        // 3. Si es una configuracion nueva o el nombre base ya existe, generar perfil
        if (!yaEscrito) {
            std::string baseName = Constant::Trim(inputs.physicalNames[p]);
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
            savedSignatures[signature] = finalMapperName;
            perfilesEscritos.insert(finalMapperName);

            // Escribir bloque de configuracion
            fileConfigJoystick.push_back("name=" + finalMapperName);
            
            std::string btns = "btns=";
            for (int i = 0; i < MAX_SDL_BUTTONS; i++)
                btns += Constant::intToString(inputs.mapperCore.sdlToBtn[p][i]) + (i < MAX_SDL_BUTTONS - 1 ? "," : "");
            fileConfigJoystick.push_back(btns);

            std::string hats = "hats=";
            for (int i = 0; i < MAX_SDL_HAT_VALUES; i++)
                hats += Constant::intToString(inputs.mapperCore.sdlToHat[p][i]) + (i < MAX_SDL_HAT_VALUES - 1 ? "," : "");
            fileConfigJoystick.push_back(hats);

            std::string axis = "axis=";
            for (int i = 0; i < MAX_SDL_AXIS_DIRS; i++)
                axis += Constant::intToString(inputs.mapperCore.sdlToAxis[p][i]) + (i < MAX_SDL_AXIS_DIRS - 1 ? "," : "");
            fileConfigJoystick.push_back(axis);

            /* Fuente fisica de cada una de las 8 direcciones analogicas. Va aqui y no
             * con anal=/joytype= porque, al contrario que esos dos, no depende del
             * core: es una preferencia del mando, como btns= o axis=. Clave nueva;
             * los ficheros antiguos no la traen y al cargarlos se respetan los
             * defaults (ver 'hasAnlg' en loadButtonsRetro). */
            std::string anlg = "anlg=";
            for (int i = 0; i < ANALOG_TARGETS; i++)
                anlg += Constant::intToString(inputs.mapperCore.analogDst[p][i]) + (i < ANALOG_TARGETS - 1 ? "," : "");
            fileConfigJoystick.push_back(anlg);

			//El tipo de dispositivo si depende del core, asi que solo va en los .joy.
			//"anal=" (el viejo axisAsPad) ya no se escribe: ahora cada direccion de
			//stick lleva su destino en anlg=. Se sigue leyendo por compatibilidad.
			if (!hotkeysAndFrontend){
				int joyType = inputs.joyTypeIdx[p];
				fileConfigJoystick.push_back("joytype=" + Constant::TipoToStr(joyType));
			}
            fileConfigJoystick.push_back(""); // Linea en blanco
        }
    }

    /* 4. Perfiles de mandos que ahora NO estan conectados: se copian tal cual.
     * Es lo que permite tener a la vez el perfil del mando de SNES y el de Xbox en
     * el mismo fichero aunque solo se pueda guardar con uno enchufado cada vez. */
    for (std::size_t i = 0; i < prevOrden.size(); i++) {
        const std::string &nombre = prevOrden[i];
        if (perfilesEscritos.count(nombre)) continue;   /* sustituido por el de arriba */
        if (nombre.empty() || nombre == "None") continue;

        fileConfigJoystick.push_back("name=" + nombre);
        std::vector<std::string> &bloque = prevBloques[nombre];
        for (std::size_t l = 0; l < bloque.size(); l++) {
            fileConfigJoystick.push_back(bloque[l]);
        }
        fileConfigJoystick.push_back(""); // Linea en blanco
    }

    // 5. Seccion de asignacion por jugador
    fileConfigJoystick.push_back("[RETROPAD]");
    for (int p = 0; p < MAX_PLAYERS; p++) {
        std::string nombre = playerProfileNames[p];
        /* Puerto sin mando conectado: conservamos lo que dijera el fichero en vez de
         * machacarlo con "None". Su bloque de perfil sigue estando (bucle de arriba),
         * asi que la asignacion sigue siendo valida cuando se vuelva a enchufar. */
        if (inputs.physicalNames[p].empty()) {
            std::map<int, std::string>::iterator itPrev = prevAsignacion.find(p);
            if (itPrev != prevAsignacion.end() && !itPrev->second.empty()) {
                nombre = itPrev->second;
            }
        }
        fileConfigJoystick.push_back("player" + Constant::intToString(p) + "_name=" + nombre);
    }

	if (hotkeysAndFrontend){
		fileConfigJoystick.push_back(""); // Linea en blanco
		fileConfigJoystick.push_back("[HOTKEYS]");
		std::string btns = "btns=";
		for (int i = 0; i < MAX_SDL_BUTTONS; i++)
			btns += Constant::intToString(inputs.mapperHotkeys.sdlToBtn[0][i]) + (i < MAX_SDL_BUTTONS - 1 ? "," : "");
		fileConfigJoystick.push_back(btns);

		std::string hats = "hats=";
		for (int i = 0; i < MAX_SDL_HAT_VALUES; i++)
			hats += Constant::intToString(inputs.mapperHotkeys.sdlToHat[0][i]) + (i < MAX_SDL_HAT_VALUES - 1 ? "," : "");
		fileConfigJoystick.push_back(hats);

		std::string axis = "axis=";
		for (int i = 0; i < MAX_SDL_AXIS_DIRS; i++)
			axis += Constant::intToString(inputs.mapperHotkeys.sdlToAxis[0][i]) + (i < MAX_SDL_AXIS_DIRS - 1 ? "," : "");
		fileConfigJoystick.push_back(axis);

		fileConfigJoystick.push_back(""); // Linea en blanco
		fileConfigJoystick.push_back("[FRONTEND]");
		btns = "btns=";
		for (int i = 0; i < MAX_SDL_BUTTONS; i++)
			btns += Constant::intToString(inputs.mapperFrontend.sdlToBtn[0][i]) + (i < MAX_SDL_BUTTONS - 1 ? "," : "");
		fileConfigJoystick.push_back(btns);

		hats = "hats=";
		for (int i = 0; i < MAX_SDL_HAT_VALUES; i++)
			hats += Constant::intToString(inputs.mapperFrontend.sdlToHat[0][i]) + (i < MAX_SDL_HAT_VALUES - 1 ? "," : "");
		fileConfigJoystick.push_back(hats);

		axis = "axis=";
		for (int i = 0; i < MAX_SDL_AXIS_DIRS; i++)
			axis += Constant::intToString(inputs.mapperFrontend.sdlToAxis[0][i]) + (i < MAX_SDL_AXIS_DIRS - 1 ? "," : "");
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
* ★ OJO: esta carga PISA las asignaciones que haya en memoria, incluidas las que
* el usuario acabe de hacer en el menu y no haya guardado.
*
* No es que haga un clear() al principio -- es mas sutil: los bucles de abajo
* recorren TODOS los valores de btns=/hats=/axis=, y para cada hueco vacio llaman a
* setBtnFromSdl/setHatFromSdl/setAxisFromSdl con -1. assignValue() con coreIdx=-1
* borra la entrada directa Y la inversa (su clearPrevious del paso 3), asi que
* cualquier mapeo que este en memoria y NO en el fichero desaparece.
*
* Y esto se llama al lanzar cada juego (GameMenu::setRomPaths -> <rom>.joy, o
* config/defaults_<core>.joy, o retropad.ini). Consecuencia practica: si
* reasignas algo en el menu y entras al juego sin usar "Guardar asignaciones",
* el cambio se pierde y parece que el remapeo no funciona. Costo dos sesiones de
* diagnostico averiguarlo; el volcado del final de esta funcion esta para que no
* vuelva a pasar.
*
* La excepcion es 'anlg=' (direcciones analogicas), que solo se aplica si la
* clave aparece de verdad en el fichero: ver el flag hasAnlg de PadProfile.
*
* Que perfil recibe cada puerto se decide DESPUES de parsear el fichero entero, en el
* bucle del final: primero por nombre del mando conectado, y si no hay coincidencia,
* por el player<N>_name del fichero.
*/
bool Joystick::loadButtonsRetro(std::string ruta) {

    std::vector<std::string> lineas;
    std::map<std::string, PadProfile> profiles;
    /* Las lineas de [RETROPAD] ya no se aplican segun se leen: se acumulan aqui y se
     * resuelven al final, cuando ya se conocen todos los perfiles del fichero. */
    std::map<int, std::string> asignaciones;
    std::string currentSection = "";
    std::string currentProfileName = "";

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
                /* 'anal=' de [RETROPAD_LIST] era el viejo axisAsPad, que ya no existe:
                 * se ignora en silencio (el parser descarta las claves desconocidas).
                 * OJO: el 'anal=' de [FRONTEND] es OTRA cosa -- alimenta frontAxisAsPad,
                 * que es navegar los menus con el stick -- y ese SI se sigue leyendo. */
				else if (line.find("joytype=") == 0)
					profiles[currentProfileName].joyTypeIdx = Constant::strToTipo<int>(line.substr(8));
				else if (line.find("anlg=") == 0) {
					profiles[currentProfileName].anlg = Constant::splitInt(line.substr(5), ',');
					profiles[currentProfileName].hasAnlg = true;
				}
            }
        } else if (currentSection == "[RETROPAD]") {
            // Formato: player0_name=Xbox Controller
            std::size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string key = line.substr(0, eqPos);

                /* El numero de jugador sale de la propia clave: "player2_name" -> 2.
                 *
                 * Antes salia de un contador y 'key' se extraia sin llegar a usarse, con
                 * dos consecuencias: (a) un fichero con las lineas desordenadas o con
                 * algun jugador ausente se cargaba desplazado, porque las lineas "=None"
                 * tambien consumian posicion; y (b) 'int p = contador' iba DESPUES del
                 * incremento, asi que el jugador 0 solo se cargaba si el nombre del
                 * perfil casaba con el del mando conectado al puerto 0 -- con un .joy
                 * hecho con otro mando, el jugador 0 se quedaba sin cargar y el resto
                 * subia una posicion. */
                int p = -1;
                for (std::size_t c = 0; c < key.size(); c++) {
                    if (key[c] >= '0' && key[c] <= '9') {
                        p = key[c] - '0';
                        break;
                    }
                }
                if (p < 0 || p >= MAX_PLAYERS) continue;   /* clave sin numero util */

                asignaciones[p] = line.substr(eqPos + 1);
            }
        } else if (currentSection == "[HOTKEYS]" || currentSection == "[FRONTEND]") {
            auto& targetMapper = (currentSection == "[HOTKEYS]") ? inputs.mapperHotkeys : inputs.mapperFrontend;
            if (line.find("btns=") == 0) {
                std::vector<int> v = Constant::splitInt(line.substr(5), ',');
                for (std::size_t j = 0; j < MAX_SDL_BUTTONS && j < v.size(); j++) targetMapper.setBtnFromSdl(0, j, v[j]);
            } else if (line.find("hats=") == 0) {
                std::vector<int> v = Constant::splitInt(line.substr(5), ',');
                for (std::size_t j = 0; j < MAX_SDL_HAT_VALUES && j < v.size(); j++) targetMapper.setHatFromSdl(0, j, v[j]);
            } else if (line.find("axis=") == 0) {
                std::vector<int> v = Constant::splitInt(line.substr(5), ',');
                for (std::size_t j = 0; j < MAX_SDL_AXIS_DIRS && j < v.size(); j++) targetMapper.setAxisFromSdl(0, j, v[j]);
            } else if (line.find("anal=") == 0){
				//Set a proper option for the frontend and don't rely only with the option assigned to the core
				inputs.frontAxisAsPad = (line.substr(5) == "1");
			}
        }
    }

	/* --- Que perfil recibe cada puerto -------------------------------------------
	 * 1) Si hay un perfil cuyo name= es EXACTAMENTE el nombre del mando conectado,
	 *    ese. Asi conviven en el mismo fichero el perfil del mando de SNES y el del
	 *    de Xbox: se recupera el que toque segun lo que haya enchufado.
	 * 2) Si no, lo que diga player<N>_name.
	 *
	 * El original comparaba con find() -- subcadena -- y solo para el puerto 0. Aqui
	 * la comparacion es exacta, para que "Mando" no case con "Mando inalambrico".
	 *
	 * Con DOS MANDOS DEL MISMO MODELO el nombre no distingue cual es cual, asi que en
	 * ese caso se salta el paso 1 y manda el fichero: si no, los dos puertos cargarian
	 * el mismo perfil. */
	for (int p = 0; p < MAX_PLAYERS; p++) {
		std::string elegido = "";
		const std::string &fisico = inputs.physicalNames[p];

		if (!fisico.empty() && profiles.count(fisico)) {
			bool nombreUnico = true;
			for (int q = 0; q < MAX_PLAYERS; q++) {
				if (q != p && inputs.physicalNames[q] == fisico) { nombreUnico = false; break; }
			}
			if (nombreUnico) elegido = fisico;
		}

		if (elegido.empty()) {
			std::map<int, std::string>::iterator itAsig = asignaciones.find(p);
			if (itAsig != asignaciones.end() && profiles.count(itAsig->second)) {
				elegido = itAsig->second;
			}
		}

		if (elegido.empty()) continue;   /* nada que aplicar a este puerto */

		{
			PadProfile& pf = profiles[elegido];
			/* El bucle esta acotado por el minimo entre el tamano del array y lo que
			 * traiga el fichero, asi que un .joy del formato anterior (31/31/31) se
			 * carga igual: los indices que sobran (boton >= 24, mascara de hat >= 16)
			 * no los usa nadie. */
			for (std::size_t j = 0; j < MAX_SDL_BUTTONS && j < pf.btns.size(); j++)    inputs.mapperCore.setBtnFromSdl(p, j, pf.btns[j]);
			for (std::size_t j = 0; j < MAX_SDL_HAT_VALUES && j < pf.hats.size(); j++) inputs.mapperCore.setHatFromSdl(p, j, pf.hats[j]);
			for (std::size_t j = 0; j < MAX_SDL_AXIS_DIRS && j < pf.axis.size(); j++)  inputs.mapperCore.setAxisFromSdl(p, j, pf.axis[j]);
			/* Solo si el fichero traia la clave: si no, se respetan los defaults de
			 * configMapperRetro en vez de dejarlo todo a -1. */
			if (pf.hasAnlg) {
				for (std::size_t j = 0; j < ANALOG_TARGETS && j < pf.anlg.size(); j++)
					inputs.mapperCore.setAnalogDst(p, j, pf.anlg[j]);
			}
			inputs.names[p] = elegido;
			//First assign the joystick type...
			inputs.joyTypeIdx[p] = pf.joyTypeIdx;
			//... and afterwards, make sure is not RETRO_DEVICE_NONE if there are more options
			//this is needed for dosbox-pure, to assign the default option to a joystick controller
			getCkeckedJoyTypeIndex(p);

			LOG_DEBUG("loadButtonsRetro: puerto %d ('%s') -> perfil '%s'", p,
				fisico.empty() ? "sin mando" : fisico.c_str(), elegido.c_str());
		}
	}

	/* Volcado del mapeo del core que queda tras cargar: solo jugador 0 y solo lo
	 * asignado. Es lo que hace falta para distinguir "el .joy no traia esa
	 * asignacion" de "la trae y el core no la usa" cuando un remapeo no responde. */
	LOG_DEBUG("loadButtonsRetro: '%s' -> mapeo del core (jugador 0)", ruta.c_str());
	for (int t = 0; t < MAX_TARGETS; t++){
		const int b = inputs.mapperCore.getSdlBtn(0, t);
		const int h = inputs.mapperCore.getSdlHat(0, t);
		const int a = inputs.mapperCore.getSdlAxis(0, t);
		if (b > -1 || h > -1 || a > -1)
			LOG_DEBUG("   destino %d: sdlBtn %d, sdlHat %d, sdlAxis %d", t, b, h, a);
	}
	for (int s = 0; s < ANALOG_TARGETS; s++){
		LOG_DEBUG("   analogico slot %d: fuente %d", s, inputs.mapperCore.getAnalogDst(0, s));
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
                if ((unsigned int)sdlBtn < MAX_SDL_BUTTONS) {
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
                if (btn < MAX_SDL_BUTTONS) {
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
                /* Una sola cota: MAX_ANALOG_AXIS acota a la vez g_analog_state[p][axis]
                 * y axis_state[p][axis*2+signo], porque axis_state mide justo el doble
                 * (MAX_SDL_AXIS_DIRS). Antes hacia falta ademas "axis >= MAX_AXIS/2"
                 * porque axis_state median 31, un numero impar sin relacion con los ejes. */
                if (axis >= MAX_ANALOG_AXIS) break;
				bool combinedAxis = false;
				#ifndef _XBOX
				//En xbox los gatillos L2 y R2 no son ejes. Se comportan como botones, al menos en la 
				//libreria de xbox 360 de Lantus, por lo que no hace falta hacer esto para forzar a 
				//que los gatillos se comporten como botones siempre
				combinedAxis = (axis == XBOX_COMBINED_TRIGGER_AXIS); 
				#endif

				/* En los menus manda frontAxisAsPad (navegar con el stick). Dentro del
				 * juego ya no hay interruptor global: cada direccion con nombre lleva
				 * su propio destino en mapperCore.analogDst, y se resuelve mas abajo.
				 * El eje 2 de Windows (gatillos combinados) no es de ningun stick con
				 * nombre, asi que sigue digitalizandose por la via de siempre. */
				const bool isPadInput = gameStatus == EMU_STARTED ? combinedAxis : inputs.frontAxisAsPad;

                {
					int32_t raw = event.jaxis.value;
                    // [XBOX360] Clamp SIMETRICO a +-32767 (estandar libretro/RetroArch). El unico
                    // valor problematico es INT16_MIN (-32768): al golpear el stick a fondo
                    // (izquierda rapida) SDL entrega -32768, y los cores que NIEGAN el eje
                    // para mapeos invertidos (dosbox-pure, meta=-1) calculan -(-32768), que
                    // NO cabe en int16 -> se queda -32768 y el input "se desactiva" en esa
                    // direccion. El positivo ya llega como maximo a +32767. Rango completo
                    // simetrico (lo que espera pcsxr-360; su "andar vs correr" era del core).
					if (raw < -32767) raw = -32767;

                    // El valor crudo se guarda SIEMPRE. Antes esto era el 'else' de
                    // isPadInput, asi que con "Analog pad" activado g_analog_state dejaba de
                    // actualizarse y se quedaba CONGELADO en el ultimo valor: mover el stick a
                    // fondo y activarlo dejaba al core viendo deflexion maxima para siempre.
                    // Ademas, teniendolo siempre al dia, el remapeo analogico puede
                    // convivir con las direcciones convertidas en boton o cruceta.
                    inputs.g_analog_state[p][axis] = (int16_t)raw;

                    if (isPadInput) {
                        // Pre-calculamos los índices para evitar multiplicar por 2 varias veces
                        const int idxNeg = axis << 1;      // axis * 2
                        const int idxPos = idxNeg | 1;     // axis * 2 + 1
                        const Sint16 val = event.jaxis.value;
                        bool* axisState = inputs.axis_state[p];
                        const bool wasPos = axisState[idxPos];
                        const bool wasNeg = axisState[idxNeg];

                        // Usamos una lógica más plana para el compilador
                        axisState[idxPos] = (val >  DEADZONE);
                        axisState[idxNeg] = (val < -DEADZONE);

                        /* Solo en las TRANSICIONES, para no inundar el log. */
                        if (axisState[idxPos] != wasPos || axisState[idxNeg] != wasNeg) {
                            LOG_DEBUG("axis_state: mando %u eje %u valor %d -> virtual %d=%d, %d=%d",
                                p, axis, (int)val, idxNeg, axisState[idxNeg] ? 1 : 0,
                                idxPos, axisState[idxPos] ? 1 : 0);
                        }
                    }

                    /* Destino de cada una de las dos direcciones de este eje. Si es un
                     * boton o una posicion de cruceta, se enciende su bit simulado y a
                     * partir de ahi getCoreBtn/getCoreHat no lo distinguen de una
                     * pulsacion real. Si es otra direccion de eje no hay nada que hacer
                     * aqui: lo resuelve la lectura (getAnalogTowards). */
                    for (int sign = 0; sign < 2; sign++) {
                        const int virt = (int)(axis << 1) | sign;
                        const int slot = t_joy_mapper::analogSlotOfVirtual(virt);
                        int dst;
                        bool active;
                        if (slot < 0) continue;   /* eje que no es de un stick con nombre */

                        dst = inputs.mapperCore.getAnalogDst(p, slot);
                        if (dst < 0) continue;

                        active = sign ? (raw > DEADZONE) : (raw < -DEADZONE);

                        if (dst >= ANALOG_DST_HAT_BASE) {
                            const int h = dst - ANALOG_DST_HAT_BASE;
                            if (h >= 0 && h < MAX_SDL_HAT_VALUES && inputs.axisSimHat[p][h] != active) {
                                inputs.axisSimHat[p][h] = active;
                                LOG_DEBUG("analogDst: mando %u virtual %d -> hat %d = %d", p, virt, h, active ? 1 : 0);
                            }
                        } else if (dst >= ANALOG_DST_BTN_BASE) {
                            const int b = dst - ANALOG_DST_BTN_BASE;
                            if (b >= 0 && b < MAX_SDL_BUTTONS && inputs.axisSimBtn[p][b] != active) {
                                inputs.axisSimBtn[p][b] = active;
                                LOG_DEBUG("analogDst: mando %u virtual %d -> boton %d = %d", p, virt, b, active ? 1 : 0);
                            }
                        }
                    }
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

				inputs.last_key_processed.key = retro_key;
				inputs.last_key_processed.keyMod = retro_mod;
				inputs.last_key_processed.unicode = character;
				inputs.last_key_processed.keyjoydown = false;

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