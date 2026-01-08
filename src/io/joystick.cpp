#include <SDL.h>
#include <io/joystick.h>
#include <io/joymapper.h>
#include <const/constant.h>

Joystick::Joystick(){
	ignoreButtonRepeats = false;
	this->w = 0;
    this->h = 0;
	lastSelectPress = 0;
	clearEvento(&lastEvento);
    gestorCursor = new CursorGestor();
    setCursor(cursor_arrow);

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

	mPrevAxisValues = new std::map<int, int>[mNumJoysticks];
	mPrevHatValues = new std::map<int, int>[mNumJoysticks];

    // Abrir todos los mandos disponibles hasta el límite de jugadores
    for (int i = 0; i < mNumJoysticks && i < MAX_PLAYERS; i++) {
        g_joysticks[i] = SDL_JoystickOpen(i);
        if (g_joysticks[i]) {
            LOG_DEBUG("Mando %d abierto: %s\n", i, SDL_JoystickName(i));
        }

		axis = SDL_JoystickNumAxes(g_joysticks[i]);
        hats = SDL_JoystickNumHats(g_joysticks[i]);
        //cout << "hay " + Constant::TipoToStr(axis) + " axis en el joystick: " + Constant::TipoToStr(i) << endl;
        for(int k = 0; k < axis; k++){
            mPrevAxisValues[i][k] = 0;
        }
        for(int k = 0; k < hats; k++){
            mPrevHatValues[i][k] = 0;
        }
    }
	return true;
}

void Joystick::resetAllValues(){
	int axis = 0;
    int hats = 0;
	// Abrir todos los mandos disponibles hasta el límite de jugadores
    for (int i = 0; i < mNumJoysticks && i < MAX_PLAYERS; i++) {
		axis = SDL_JoystickNumAxes(g_joysticks[i]);
        hats = SDL_JoystickNumHats(g_joysticks[i]);
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
                    evento.joy = JoyMapper::getJoyMapper(event.jbutton.button);
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

                if (event.jhat.value & SDL_HAT_UP){
                    evento.joy = JOYHATOFFSET + event.jhat.value;
                } else if (event.jhat.value & SDL_HAT_DOWN){
                    evento.joy = JOYHATOFFSET + event.jhat.value;
                } else if (event.jhat.value & SDL_HAT_LEFT){
                    evento.joy = JOYHATOFFSET + event.jhat.value;
                } else if (event.jhat.value & SDL_HAT_RIGHT){
                    evento.joy = JOYHATOFFSET + event.jhat.value;
                }

                if (event.jhat.value == 0) evento.keyjoydown = false;
                else {
                    evento.keyjoydown = true;
                    lastKeyDown = SDL_GetTicks();  //reseteo del keydown
                }
                lastEvento = evento;    //Guardamos el ultimo evento que hemos lanzado

                break;
            case SDL_JOYAXISMOTION:
                if((abs(event.jaxis.value) > DEADZONE) != (abs(mPrevAxisValues[event.jaxis.which][event.jaxis.axis]) > DEADZONE))
                {
                    int normValue;
                    evento.isJoy = true;

                    if(abs(event.jaxis.value) <= DEADZONE){
                        normValue = 0;
                        evento.keyjoydown = false;
                    } else {
                        if(event.jaxis.value > 0)
                            normValue = 1;
                        else
                            normValue = -1;

                        evento.keyjoydown = true;
                        lastKeyDown = SDL_GetTicks();  //reseteo del keydown
                    }

                    int valor = (abs(normValue) << 4 | event.jaxis.axis) * normValue;
                    evento.joy = JOYAXISOFFSET + valor;
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
                if (evento.keyMod & KMOD_LCTRL && evento.key == SDLK_c) evento.quit = true;
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