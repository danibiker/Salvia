#include <gfx/SDL_gfxPrimitives.h>
#include <menus/gestormenus.h>
#include <const/constant.h>
#include <gfx/gfx_utils.h>
#include <font/fonts.h>
#include <math.h>
#include <const/menuconst.h>
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
}

GestorMenus::~GestorMenus() {
    for(size_t i = 0; i < todosLosMenus.size(); i++) delete todosLosMenus[i];
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
void GestorMenus::inicializar(CfgLoader *refConfig) {
    // 1. Crear contenedores de menús
    menuRaiz = new Menu("Opciones");
    Menu* menuVideo = new Menu("Vídeo", menuRaiz);
	Menu* menuEmulation = new Menu("Emulación", menuRaiz);
	Menu* menuEntrada = new Menu("Entrada", menuRaiz);
        
    todosLosMenus.push_back(menuRaiz);
    todosLosMenus.push_back(menuVideo);
	todosLosMenus.push_back(menuEmulation);
	todosLosMenus.push_back(menuEntrada);

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

	Menu* menuAsignaciones = new Menu("Asignaciones de Retropad", menuEntrada);
	Menu* menuHotkeys = new Menu("Teclas rápidas", menuEntrada);
	todosLosMenus.push_back(menuAsignaciones);
	todosLosMenus.push_back(menuHotkeys);

	menuEntrada->opciones.push_back(new OpcionSubMenu("Asignaciones de Retropad", menuAsignaciones));
	menuEntrada->opciones.push_back(new OpcionSubMenu("Teclas rápidas", menuHotkeys));


	for (int controlId = 0; controlId < MAX_PLAYERS; controlId++){
		std::string controlStr = "Controles del puerto " + Constant::TipoToStr(controlId + 1);
		Menu* menuControlesPuerto = new Menu(controlStr, menuAsignaciones);
		addControlerButtons(menuControlesPuerto, controlId);
		menuAsignaciones->opciones.push_back(new OpcionSubMenu(controlStr, menuControlesPuerto));
		todosLosMenus.push_back(menuControlesPuerto);
	}

    // 3. Poblar Menú Principal
    menuRaiz->opciones.push_back(new OpcionSubMenu("Configuración vídeo", menuVideo));
	menuRaiz->opciones.push_back(new OpcionSubMenu("Configuración emulación", menuEmulation));
	menuRaiz->opciones.push_back(new OpcionSubMenu("Entrada", menuEntrada));

    // Establecer estado inicial
    menuActual = menuRaiz;
	resetIndexPos();
}

void GestorMenus::addControlerButtons(Menu*& menu, int controlId){
	int num_port_buttons = sizeof(configurablePortButtons) / sizeof(configurablePortButtons[0]);
	int num_port_hats = sizeof(configurablePortHats) / sizeof(configurablePortHats[0]);

	static int notfound = -1;
	std::string text;

	const int nBtns = Joystick::buttonsMapperLibretro[controlId].nButtons;
	const int nHats = Joystick::buttonsMapperLibretro[controlId].nHats;

	for (int i=0; i < num_port_hats; i++){
		std::string text = configurablePortHatsStr[i];
		//int btn = findBtnPad(controlId, configurablePortHats[i]);
		if (nHats > 0){
			menu->opciones.push_back(new OpcionKey(text, &Joystick::buttonsMapperLibretro[controlId], controlId, configurablePortHats[i], KEY_JOY_HAT, "Hat: "));	
		}
	}

	for (int i=0; i < num_port_buttons; i++){
		std::string text = configurablePortButtonsStr[i];
		int btn = findBtnPad(controlId, configurablePortButtons[i]);
		if (nBtns > 0){
			menu->opciones.push_back(new OpcionKey(text, &Joystick::buttonsMapperLibretro[controlId], controlId, btn, KEY_JOY_BTN, "Btn: "));	
		} 
	}
}

/*int GestorMenus::findBtnHat(int controlId, int btn){
	int nHat = Joystick::buttonsMapperLibretro[controlId].nHats;
	for (int i=0; i < nHat; i++){
		if (Joystick::buttonsMapperLibretro[controlId].hats[i].joy == btn){
			return i;
		}
	}
	return -1;
}*/

int GestorMenus::findBtnPad(int controlId, int btn){
	int nBut = Joystick::buttonsMapperLibretro[controlId].nButtons;
	for (int i=0; i < nBut; i++){
		if (Joystick::buttonsMapperLibretro[controlId].buttons[i].joy == btn){
			return i;
		}
	}
	return -1;
}

// Lógica de navegación Arriba/Abajo
void GestorMenus::navegar(int dir) { // -1 o 1
    if (!menuActual || status == POLLING_INPUTS ) return;
    int num = (int)menuActual->opciones.size();
	if (num > 0){
		menuActual->seleccionado = (menuActual->seleccionado + dir + num) % num;
	}
}

/**
*
*/
void GestorMenus::updateButton(int sdlbtn){
	if (status != POLLING_INPUTS) return;
	Opcion* opt = menuActual->opciones[menuActual->seleccionado];

	if (opt->tipo == OPC_KEY) {
		OpcionKey* k = static_cast<OpcionKey*>(opt);
		if (k && k->valor && k->btn >= 0) {
			if (k->tipoKey == KEY_JOY_BTN){
				// 1. Obtener el ID de Libretro que queremos asignar (ej: RETRO_DEVICE_ID_JOYPAD_A)
				int retroID = k->valor->buttons[k->btn].joy; 
				// Limpiar el mapeo antiguo para evitar que dos botones hagan lo mismo
				k->valor->buttons[k->btn].joy = k->valor->buttons[sdlbtn].joy; 
				//k->valor->buttons[k->btn].index = k->btn;
				k->changeAsked = false;
				k->lastTimeAsked = 0;
				status = NORMAL;
				LOG_DEBUG("Change to %d, previous %d, retrobtn %d", sdlbtn, k->valor->buttons[k->btn].index, retroID);
				// 3. Asignar al nuevo índice físico de SDL
				k->valor->setButton(sdlbtn, retroID);
				menuActual->opciones.clear();
				addControlerButtons(menuActual, k->gamepadId);
				//LOG_DEBUG("Accion Libretro %d ahora mapeada al boton SDL %d", retroID, sdlbtn);
			} else if (k->tipoKey == KEY_JOY_HAT){
				// Extraemos la dirección activa del Hat (limpiamos otros bits si fuera necesario)
				Uint8 hatDir = (Uint8)(sdlbtn & (SDL_HAT_UP | SDL_HAT_DOWN | SDL_HAT_LEFT | SDL_HAT_RIGHT));
				if (hatDir > 0) {
					k->valor->setHat(k->valor->buttons[k->btn].index, hatDir);
				}

				k->changeAsked = false;
				k->lastTimeAsked = 0;
				status = NORMAL;
			}
		}
	}
}

// Lógica para cambiar valores (Izquierda / Derecha)
void GestorMenus::cambiarValor(int dir) {
	if (status == POLLING_INPUTS || menuActual->opciones.size() == 0) return;

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
	} else if (opt->tipo == OPC_KEY) {
		OpcionKey* k = (OpcionKey*)opt;
		k->changeAsked = true;
		k->lastTimeAsked = SDL_GetTicks();
		status = POLLING_INPUTS;
	}
}

// Lógica para confirmar (Botón A)
void GestorMenus::confirmar() {
	if (status == POLLING_INPUTS || menuActual->opciones.size() == 0) return;

    Opcion* opt = menuActual->opciones[menuActual->seleccionado];
    if (opt->tipo == OPC_SUBMENU) {
        menuActual = ((OpcionSubMenu*)opt)->destino;
		resetIndexPos();
    } else if (opt->tipo == OPC_BOOLEANA) {
        cambiarValor(1);
	} else if (opt->tipo == OPC_KEY) {
        cambiarValor(1);
    }
}

// Lógica para volver (Botón B)
void GestorMenus::volver() {
	if (status == POLLING_INPUTS) return;

    if (menuActual->padre != NULL) {
        menuActual = menuActual->padre;
		resetIndexPos();
    }
}

Menu* GestorMenus::obtenerMenuActual() {
	return menuActual; 
}

void GestorMenus::draw(SDL_Surface *video_page){
	static const int bkg = SDL_MapRGB(video_page->format, bkgMenu.r, bkgMenu.g, bkgMenu.b);
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
        const int screenPos = i - this->iniPos;
        const int fontHeightRect = screenPos * face_h;
        const int lineBackground = -1;
        SDL_Color lineTextColor = i == this->curPos ? black : white;

		std::string line;
		if (option->tipo == OPC_BOOLEANA){
			line = option->titulo + " " + std::string(*((OpcionBool *)option)->valor ? "Y" : "N");
		} else if (option->tipo == OPC_LISTA){
			int indice = *((OpcionLista *)option)->indice;
			line = option->titulo + " <" + ((OpcionLista *)option)->items.at(indice) + ">";
		} else if (option->tipo == OPC_SUBMENU){
			line = option->titulo + " >";
		} else if (option->tipo == OPC_INT){
			line = option->titulo;// + " " + ((OpcionInt *)option)->description + Constant::intToString(*((OpcionInt *)option)->valor);
		} else if (option->tipo == OPC_KEY){
			line = option->titulo;
		}

        //Drawing a faded background selection rectangle
        if (i == this->curPos){
            int y = this->getY() + fontHeightRect;
            //Gaining some extra fps when the screen resolution is low
			SDL_Rect rectElem = {this->getX() + marginX, y, this->getW() - 2 * marginX, face_h};
            if (video_page->h >= 480){
				DrawRectAlpha(video_page, rectElem, bkgMenu, 190);
            } else {
                lineTextColor = white;
            }
			rect(video_page, rectElem.x - 1, rectElem.y - 1, rectElem.x + rectElem.w, rectElem.y + rectElem.h, bkgMenu);
        }
                
        Constant::drawTextTransparent(video_page, fontMenu, line.c_str(), this->getX() + marginX, 
                    this->getY() + fontHeightRect, lineTextColor, lineBackground);

		if (option->tipo == OPC_KEY && !((OpcionKey *)option)->description.empty()){
			std::string str = "";
			OpcionKey *opt = ((OpcionKey *)option);

			if (opt->tipoKey == KEY_JOY_BTN){
				if (opt->changeAsked && SDL_GetTicks() - opt->lastTimeAsked < 4000){
					str = "Esperando pulsación de tecla en " + Constant::intToString( (5000 - (SDL_GetTicks() - opt->lastTimeAsked)) / 1000 ) + " s";
				} else if (opt->changeAsked) {
					str = opt->description + Constant::intToString(opt->valor->buttons[opt->btn].index);
					opt->changeAsked = false;
					opt->lastTimeAsked = 0;
					status = NORMAL;
				} else {
					str = opt->description + Constant::intToString(opt->valor->buttons[opt->btn].index);
				}
			} else if (opt->tipoKey == KEY_JOY_HAT) {
				if (opt->changeAsked && SDL_GetTicks() - opt->lastTimeAsked < 4000){
					str = "Esperando pulsación de tecla en " + Constant::intToString( (5000 - (SDL_GetTicks() - opt->lastTimeAsked)) / 1000 ) + " s";
				} else if (opt->changeAsked) {
					str = opt->description + Constant::intToString(opt->valor->hats[opt->btn].index);
					opt->changeAsked = false;
					opt->lastTimeAsked = 0;
					status = NORMAL;
				} else {
					str = opt->description + Constant::intToString(opt->valor->hats[opt->btn].index);
				}
			}

			int pixelDato;
			TTF_SizeText(fontMenu, str.c_str(), &pixelDato, NULL);

			Constant::drawTextTransparent(video_page, fontMenu, str.c_str(), this->getX() + this->getW() - marginX - pixelDato - 1, 
                    this->getY() + fontHeightRect, lineTextColor, lineBackground);
		}
    }
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

void GestorMenus::nextPos(){
	this->navegar(1);
	this->curPos = menuActual->seleccionado;
}

void GestorMenus::prevPos(){
	this->navegar(-1);
	this->curPos = menuActual->seleccionado;
}

void GestorMenus::clearSelectedText(){
    if (imgText != NULL){
		SDL_FreeSurface(imgText);
        imgText = NULL;
    }
}