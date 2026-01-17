#include <SDL.h>
#include <io/joystick.h>
#include <io/joymapper.h>
#include <io/filelist.h>
#include <const/constant.h>

t_joy_retro_inputs Joystick::buttonsMapperLibretro[MAX_PLAYERS];
t_joy_inputs Joystick::buttonsMapperFrontend;

Joystick::Joystick(){
	ignoreButtonRepeats = false;
	this->w = 0;
    this->h = 0;
	lastSelectPress = 0;
	clearEvento(&lastEvento);
    gestorCursor = new CursorGestor();
	setCursor(cursor_hidden);

	for (int i=0; i < MAX_PLAYERS; i++){
		g_joysticks[i] = NULL;
		for (int j=0; j < RETRO_DEVICE_ID_JOYPAD_R3 + 1; j++){
			g_joy_state[i][j] = false;
		}
	}
}

Joystick::~Joystick(){
	close_joysticks();
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

    // Abrir todos los mandos disponibles hasta el límite de jugadores
    for (int joyId = 0; joyId < MAX_PLAYERS; joyId++) {
        g_joysticks[joyId] = SDL_JoystickOpen(joyId);
        
		if (g_joysticks[joyId]) {
            LOG_DEBUG("Mando %d abierto: %s\n", joyId, SDL_JoystickName(joyId));
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
		buttonsMapperLibretro[joyId].buttons = new t_retro_input [buttons];
		buttonsMapperLibretro[joyId].retroButtons = new t_retro_input [RETRO_DEVICE_ID_JOYPAD_R3 + 1];

		buttonsMapperLibretro[joyId].hats = new t_retro_input [RETRO_DEVICE_ID_JOYPAD_R3 + 1];
		buttonsMapperLibretro[joyId].axis = new t_retro_input [axis * 2];
		buttonsMapperLibretro[joyId].nButtons = buttons;
		buttonsMapperLibretro[joyId].nAxis = axis;
		buttonsMapperLibretro[joyId].nHats = RETRO_DEVICE_ID_JOYPAD_R3 + 1;
		loadButtonsEmupad(joyId);
		
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

void Joystick::loadButtonsEmupad(int joyId){

	int num_port_buttons = sizeof(configurablePortButtons) / sizeof(configurablePortButtons[0]);

	//Cada Posicion del array debe cuadrar con sdl
	//TODO: Ajustar para el numero de botones correspondiente a cada joystick
	for (int i=0; i < buttonsMapperLibretro[joyId].nButtons; i++){
		buttonsMapperLibretro[joyId].setButton(i, i < num_port_buttons ? configurablePortButtons[i] : -1);
	}

	if (buttonsMapperLibretro[joyId].nAxis >= 2){
		buttonsMapperLibretro[joyId].setAxis(0, RETRO_DEVICE_ID_ANALOG_X);
		buttonsMapperLibretro[joyId].setAxis(1, RETRO_DEVICE_ID_ANALOG_Y);
	}

	buttonsMapperLibretro[joyId].setHat(RETRO_DEVICE_ID_JOYPAD_UP, SDL_HAT_UP);
	buttonsMapperLibretro[joyId].setHat(RETRO_DEVICE_ID_JOYPAD_RIGHT, SDL_HAT_RIGHT);
	buttonsMapperLibretro[joyId].setHat(RETRO_DEVICE_ID_JOYPAD_DOWN, SDL_HAT_DOWN);
	buttonsMapperLibretro[joyId].setHat(RETRO_DEVICE_ID_JOYPAD_LEFT, SDL_HAT_LEFT);
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

void Joystick::cargarValoresEnArray(int *&array, std::string str, int maxValues){
	std::vector<std::string> v = Constant::splitChar(str, ',');
	for (int i=0; i < v.size() && i < maxValues; i++){
		array[i] = Constant::strToTipo<int>(v[i]);
	}
}

void Joystick::saveButtonsFrontend(std::string rutaIni){
	std::vector<std::string> fileConfigJoystick;

	fileConfigJoystick.push_back("[FRONTEND]");
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
                    evento.isJoy = true;
                    evento.keyjoydown = true;
                    lastEvento = evento;    //Guardamos el ultimo evento que hemos lanzado desde el teclado
                    lastKeyDown = SDL_GetTicks();  //reseteo del keydown
					longKeyDown = lastKeyDown;
                }
                break;
            case SDL_JOYBUTTONUP:
                lastEvento = evento;
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
		if (longKeyDown > 0 && now - longKeyDown > LONGKEYTIMEOUT){
			LOG_DEBUG("Long press detected for key %d\n", lastEvento.joy);
			lastEvento.longKeyPress[lastEvento.joy] = true;
			longKeyDown = 0;
		} 

		//printf("now %d vs %d\n", now, lastKeyDown + KEYDOWNSPEED + retrasoTecla);
        if (now > lastKeyDown + KEYDOWNSPEED + retrasoTecla){
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