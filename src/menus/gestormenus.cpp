#include <gfx/SDL_gfxPrimitives.h>
#include <menus/gestormenus.h>
#include <const/constant.h>
#include <gfx/gfx_utils.h>
#include <font/fonts.h>
#include <math.h>
#include <const/menuconst.h>

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
        
    todosLosMenus.push_back(menuRaiz);
    todosLosMenus.push_back(menuVideo);
	todosLosMenus.push_back(menuEmulation);

	//Poblar menu emulacion
	//Escalado de video
    std::vector<std::string> syncvals;
	for (int i=0; i < TOTAL_VIDEO_SYNC; i++){
		syncvals.push_back(syncOptionsStrings[i]);
	}
	menuEmulation->opciones.push_back(new OpcionLista("Sincronización", syncvals, &refConfig->configMain[cfg::syncMode].getIntRef()));

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

    // 3. Poblar Menú Principal
    menuRaiz->opciones.push_back(new OpcionSubMenu("Configuración vídeo", menuVideo));
	menuRaiz->opciones.push_back(new OpcionSubMenu("Configuración emulación", menuEmulation));

    // Establecer estado inicial
    menuActual = menuRaiz;
	resetIndexPos();
}

// Lógica de navegación Arriba/Abajo
void GestorMenus::navegar(int dir) { // -1 o 1
    if (!menuActual) return;
    int num = (int)menuActual->opciones.size();
    menuActual->seleccionado = (menuActual->seleccionado + dir + num) % num;
}

// Lógica para cambiar valores (Izquierda / Derecha)
void GestorMenus::cambiarValor(int dir) {
    Opcion* opt = menuActual->opciones[menuActual->seleccionado];
    if (opt->tipo == OPC_LISTA) {
        OpcionLista* l = (OpcionLista*)opt;
        int num = (int)l->items.size();
        *(l->indice) = (*(l->indice) + dir + num) % num;
    } else if (opt->tipo == OPC_BOOLEANA) {
        OpcionBool* b = (OpcionBool*)opt;
		LOG_DEBUG("cambiando de %s\n", *(b->valor) ? "S" : "N");
        *(b->valor) = !(*(b->valor));
		LOG_DEBUG("a %s\n",  *(b->valor) ? "S" : "N");
    }
}

// Lógica para confirmar (Botón A)
void GestorMenus::confirmar() {
    Opcion* opt = menuActual->opciones[menuActual->seleccionado];
    if (opt->tipo == OPC_SUBMENU) {
        menuActual = ((OpcionSubMenu*)opt)->destino;
		resetIndexPos();
    } else if (opt->tipo == OPC_BOOLEANA) {
        cambiarValor(1);
    }
}

// Lógica para volver (Botón B)
void GestorMenus::volver() {
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