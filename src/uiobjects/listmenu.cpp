#include "listmenu.h"

#include <io/dirutil.h>
#include <beans/structures.h>
#include <font/fonts.h>
#include <gfx/SDL_gfxPrimitives.h>
#include <gfx/gfx_utils.h>
#include <io/cfgloader.h>
#include <ostream>


SDL_Surface* ListMenu::imgText;
const int marginTextIcon = Icons::icon_w_add + 14;

void ListMenu::clearSelectedText(){
    if (imgText != NULL){
		SDL_FreeSurface(imgText);
        imgText = NULL;
    }
}

ListMenu::ListMenu(int screenw, int screenh){
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
    animateBkg = true;
    setObjectType(GUILISTBOX);
    setLayout(LAYSIMPLE, screenw, screenh);
    //set_trans_blender(255, 255, 255, 190);
	icons = new Icons();
	icons->loadIcons();
	selecAlphaRec = NULL;
}

ListMenu::~ListMenu(){
	LOG_DEBUG("Deleting ListMenu...");
	//Respetar el orden de limpiado en la destruccion porque sino hay problemas
	filteredGames.clear();
	listGames.clear();
	delete icons;
}

void ListMenu::clear(){
    if (listGames.size() > 0){
        listGames.clear();
		resetFilter();
	}
}

std::size_t ListMenu::getNumGames(){
    return filteredGames.size();
}

int ListMenu::getScreenNumLines(){
	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	int face_h = TTF_FontLineSkip(fontMenu);
    return face_h != 0 ? (int)std::floor((double)getH() / face_h) : 0;
}

/**
    * 
    */
void ListMenu::setLayout(int layout, int screenw, int screenh){
    this->marginY = (int) (screenh / SCREENHDIV * 1.5);
    clearSelectedText();

    if (layout == LAYBOXES){
        this->setX(0);
        this->setY(marginY);
        this->setW(screenw / 2);
        this->setH(screenh - marginY);
        this->centerText = false;
        this->layout = layout;
    } else {
        this->setX(marginX);
        this->setY(marginY);
        this->setW(screenw - marginX);
        this->setH(screenh - marginY);
        this->centerText = true;
        this->layout = layout;
    }
}

void ListMenu::applyFilter() {
    filteredGames.clear();

	bool hasMameData = !mameDatabase.empty();
	//Limpiamos todos los filtros que no tienen nada seleccionado previamente
	if (hasMameData){
		if (gameDataFields.posManufacturer == -1) 
			gameDataFields.manufacturers.clear();
		if (gameDataFields.posSystem == -1) 
			gameDataFields.systems.clear();
		if (gameDataFields.posYear == -1) 
			gameDataFields.years.clear();
	}
	

    for (auto it = listGames.cbegin(); it != listGames.cend(); ++it) {
        const auto& game = *it;
        bool include = true;

        if (game->gameData == NULL) {
            include = !gameDataFields.shouldFilter();
        } else {
            include = gameDataFields.filterManufacturer(game->gameData->manufacturer) &&
					  gameDataFields.filterYear(Constant::TipoToStr(game->gameData->year)) &&
                      gameDataFields.filterSystem(game->gameData->sourcefile) && 
					  gameDataFields.filterParent(!game->gameData->isClone());
        }

        if (include) {
			filteredGames.push_back(game.get());
			if (hasMameData && game->gameData != NULL) {
				if (gameDataFields.posManufacturer == -1)
					gameDataFields.manufacturers.push_back(game->gameData->manufacturer);
				if (gameDataFields.posYear == -1)
					gameDataFields.years.push_back(Constant::TipoToStr(game->gameData->year));
				if (gameDataFields.posSystem == -1) {
					std::string system = extractSystem(game->gameData->sourcefile);
					if (!system.empty())
						gameDataFields.systems.push_back(system);
				}
			}
		}
    }
	//Ordenamos los filtros y quitamos duplicados
	sortFilters();
}

void ListMenu::resetFilter() {
    filteredGames.clear();
    filteredGames.reserve(listGames.size());
    for (auto it = listGames.cbegin(); it != listGames.cend(); ++it) {
        filteredGames.push_back(it->get());
    }
	resetIndexPos();
}

void ListMenu::checkFilter(){
	if (gameDataFields.filterChanged()){
		LOG_DEBUG("Filter changed. Aplying...");
		applyFilter();
		resetIndexPos();
		LOG_DEBUG("Filter aplied");
	}
}

void ListMenu::sortFilters(){
	std::sort(this->listGames.begin(), this->listGames.end(), ListMenu::compareUniquePtrsFast);
	LOG_DEBUG("Sorting manufacters");
	std::sort(gameDataFields.manufacturers.begin(), gameDataFields.manufacturers.end());
	// Eliminamos duplicados
	auto itManu = std::unique(gameDataFields.manufacturers.begin(), gameDataFields.manufacturers.end());
	gameDataFields.manufacturers.erase(itManu, gameDataFields.manufacturers.end());
	LOG_DEBUG("Sorting Years");
	std::sort(gameDataFields.years.begin(), gameDataFields.years.end());
	// Eliminamos duplicados
	auto itYear = std::unique(gameDataFields.years.begin(), gameDataFields.years.end());
	gameDataFields.years.erase(itYear, gameDataFields.years.end());
	LOG_DEBUG("Sorting Systems");
	std::sort(gameDataFields.systems.begin(), gameDataFields.systems.end());
	// Eliminamos duplicados
	auto itSystem = std::unique(gameDataFields.systems.begin(), gameDataFields.systems.end());
	gameDataFields.systems.erase(itSystem, gameDataFields.systems.end());
	LOG_DEBUG("All sorted");
}

void ListMenu::draw(SDL_Surface *video_page, bool haveFocus){
    TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
    int face_h = TTF_FontLineSkip(fontMenu);
	bool staticBg = CfgLoader::configMain[cfg::animBG].valueInt != BG_TILES;
	static bool lastHaveFocus = haveFocus;

    // Guarda la posicion seleccionada del frame anterior para saber cuando cambio
    static int prevCurPos = -1;
    // Guarda el rango visible del frame anterior
    static int prevIniPos = -1;
    static int prevEndPos = -1;
	//Creamos el rect que establece la porcion del texto que se debe mostrar
	SDL_Rect srcRect;
	srcRect.y = 0;
	srcRect.w = this->getW() - 2 * this->marginX - marginTextIcon;
	srcRect.h = face_h;
	//Para controlar el tiempo de scrolling del texto
	static uint32_t lastTick = 0;
	//To scroll one letter in one second. We use the face_h because the width of 
    //a letter is not fixed.
    const float pixelsScrollFps = max(ceil(face_h / (float)textFps), 1.0f);
	const SDL_Color colorTextSel = haveFocus ? black : darkgray;
	const SDL_Color colorTextNotSel = haveFocus ? white : darkgray;

	//Creamos el rectangulo de elemento seleccionado translucido
	SDL_Rect rectElem = {0, 0, this->getW() - 2 * marginX, face_h};
	if (selecAlphaRec == NULL || selecAlphaRec->w != rectElem.w || selecAlphaRec->h != rectElem.h){
		if (selecAlphaRec != NULL){
			SDL_FreeSurface(selecAlphaRec);
		}
		Constant::createRectAlphaFilled(selecAlphaRec, rectElem, video_page->format, clBkgMenu, true);
	}

    // Si cambio la seleccion: invalida la cache del elemento anterior y del nuevo
    if (prevCurPos != -1 && prevCurPos != this->curPos) {
		if ((int)filteredGames.size() > prevCurPos){
			auto& prevGame = filteredGames[prevCurPos];
			SDL_FreeSurface(prevGame->cache);
			prevGame->cache = NULL;
		} else {
			prevCurPos = -1;
		}
        
		if ((int)filteredGames.size() > this->curPos){
			auto& curGame = filteredGames[this->curPos];
			SDL_FreeSurface(curGame->cache);
			curGame->cache = NULL;
		}
    }
    prevCurPos = this->curPos;

	if (lastHaveFocus != haveFocus){
		//Si hemos cambiado el foco, liberamos la cache de todos los elementos
		lastHaveFocus = haveFocus;
		for (int i = 0; i < (int)filteredGames.size(); i++) {
			if (filteredGames[i]->cache != NULL){
				SDL_FreeSurface(filteredGames[i]->cache);
				filteredGames[i]->cache = NULL;
			}
		}
	} else {
		// Libera cache de elementos que ya no son visibles
		if (prevIniPos != -1) {
			for (int i = prevIniPos; i < prevEndPos; i++) {
				if (i < this->iniPos || i >= this->endPos) {
					if (i < (int)filteredGames.size()) {
						//LOG_DEBUG("liberando cache %s", filteredGames[i]->shortFileName.c_str());
						SDL_FreeSurface(filteredGames[i]->cache);
						filteredGames[i]->cache = NULL;
					}
				}
			}
		}
	}
    prevIniPos = this->iniPos;
    prevEndPos = this->endPos;

	//Dibujamos el texto
    for (int i=this->iniPos; i < this->endPos; i++){
        auto& game = filteredGames.at(i);
        const int screenPos = i - this->iniPos;
        const int fontHeightRect = screenPos * face_h;
        SDL_Rect dstRectIcon = {this->getX() + marginX, this->getY() + fontHeightRect + 2, 0, 0};
        SDL_Rect dstRectWithMargin = {dstRectIcon.x + marginTextIcon, dstRectIcon.y - 2, 0, 0};
		string line = game->gameTitle.empty() ? game->longFileName : game->gameTitle;

        // Color distinto si esta seleccionado
        SDL_Color lineTextColor = i == this->curPos ? colorTextSel : colorTextNotSel;
		// Establecemos el inicio de la superficie del texto que se mostara con scroll
		srcRect.x = i == this->curPos ? (Sint16)pixelShift : 0;

		//Drawing a faded background selection rectangle
        if (i == this->curPos){
			SDL_Rect rectElemBlit = {this->getX() + marginX, this->getY() + fontHeightRect, rectElem.w, rectElem.h};
			if (haveFocus){
				SDL_BlitSurface(selecAlphaRec, NULL, video_page, &rectElemBlit);
			} else {
				rect(video_page, rectElemBlit.x, rectElemBlit.y, rectElemBlit.x + rectElemBlit.w - 1, rectElemBlit.y + rectElemBlit.h - 1, Constant::colors[clBkgMenu].sdlColor);
			}
        }

        if (game->cache == NULL) {
            // Cache no existe -> renderizar
			game->cache = TTF_RenderUTF8_Blended(fontMenu, line.c_str(), lineTextColor);
			//Huge speedup if the background is static
//#ifdef _XBOX
//			if (staticBg && i != this->curPos){
//				SDL_SetAlpha(game->cache, 0, 0);
//			}
//#endif
			//if (game->cache->format->BitsPerPixel != video_page->format->BitsPerPixel ||
			//		game->cache->format->Amask != video_page->format->Amask || 
			//		game->cache->format->Rmask != video_page->format->Rmask || 
			//		game->cache->format->Gmask != video_page->format->Gmask || 
			//		game->cache->format->Bmask != video_page->format->Bmask){
			//	SDL_Surface* convertTxt = SDL_ConvertSurface(game->cache, video_page->format, SDL_SWSURFACE);
			//	SDL_FreeSurface(game->cache);
			//	game->cache = convertTxt;
			//}
			//LOG_DEBUG("Renderizando %s", line.c_str());
            SDL_BlitSurface(game->cache, &srcRect, video_page, &dstRectWithMargin);
        } else {
            // Cache existente -> solo dibujar
            SDL_BlitSurface(game->cache, &srcRect, video_page, &dstRectWithMargin);
        }

		int txtDifWidth = game->cache->w - (selecAlphaRec->w - marginX - marginTextIcon);
		//Comprobando si el tamanyo del texto es mayor que el espacio disponible
		if (i == this->curPos && txtDifWidth > 0 && SDL_GetTicks() > lastTick + frameTimeText) {
			// Pausa de 2s al inicio (o al reiniciar el ciclo tras wrappear)
			if (pixelShift == 0) {
				lastTick = SDL_GetTicks() + waitTitleMove;
				pixelShift += 0.1f;  // sale del estado "0" para no repetir la pausa
			} else if (pixelShift + pixelsScrollFps > (float)txtDifWidth) {
				// Salta al inicio cuando el texto llega al final (sin pausa aqui)
				pixelShift = 0;
				lastTick = SDL_GetTicks();
			} else {
				// Avance normal con fmod
				pixelShift = fmod(pixelShift + pixelsScrollFps, (float)txtDifWidth);
				lastTick = SDL_GetTicks();
				// Pausa de 2s cuando estamos en el ultimo paso antes del wrap
				if (pixelShift + pixelsScrollFps >= txtDifWidth && pixelShift < txtDifWidth) {
					lastTick += waitTitleMove;
				}
			}
		}

		//Dibujamos el icono
		if (icons->icons.size() > page_white){
			SDL_Surface *cartIcon = icons->icons_carts[getCartForSystem(game->systemid)];
			if (cartIcon != NULL)
				SDL_BlitSurface(icons->icons_carts[getCartForSystem(game->systemid)], NULL, video_page, &dstRectIcon);
		}
    }
}

int ListMenu::getCartForSystem(int systemid){
	switch(systemid){
		case 1:
			return cart_genesis;
		case 2: 
			return cart_sms;
		case 3:
			return cart_nes;
		case 4:
			return cart_snes;
		case 9:
			return cart_gb;
		case 12:
			return cart_gba;
		case 19:
			return cart_32x;
		case 20:
			return cart_mcd;
		case 21:
			return cart_gg;
		case 31:
			return cart_pce;
		case 57:
			return cart_psx;
		default:
			return cart_nes;
	}
}

/**
*
*/
void ListMenu::mapFileToList(string filepath) {
    dirutil dir;
        fstream fileRomList;
        fileRomList.open(filepath, ios::in);

        if (fileRomList.is_open()){
            this->clear();
            string uri;
            string filelong;
            while(getline(fileRomList, uri)){
                if (uri.length() > 1){
                    GameFile gameFile;
					/**TODO: Esto viene del frontend de msdos, en el que no podian haber espacios en la ruta
					* En el fichero .map, tenemos la ruta y la descripcion del juego separadas por un espacio,
					* con lo que necesitamos buscar ese espacio para distinguirlos. Esto es incorrecto ahora 
					* mismo en xbox360, windows, etc.
					*/
                    std::size_t found = uri.find_first_of(" ");
                    if (found != string::npos){
                        gameFile.longFileName = uri.substr(0, found);
                        gameFile.gameTitle = Constant::Trim(Constant::replaceAll(uri.substr(found + 1), "\"", ""));;
                    }
					listGames.push_back(std::unique_ptr<GameFile>(new GameFile(gameFile)));
                }
            }
            std::sort(listGames.begin(), listGames.end(), ListMenu::compareUniquePtrs);
            resetIndexPos();
        }
        fileRomList.close();
}

// Define the comparison function
bool ListMenu::compareUniquePtrs(const std::unique_ptr<GameFile>& a,
                        const std::unique_ptr<GameFile>& b) {
    string sA = !a->gameTitle.empty() ? a->gameTitle : a->longFileName;
    string sB = !b->gameTitle.empty() ? b->gameTitle : b->longFileName;
    Constant::lowerCase(&sA);
    Constant::lowerCase(&sB);
    return sA.compare(sB) < 0;
}

bool ListMenu::compareUniquePtrsFast(const std::unique_ptr<GameFile>& a,
                                 const std::unique_ptr<GameFile>& b) {
    // Comparación directa de strings ya procesados
    return a->sortKey < b->sortKey;
}

void ListMenu::loadMameDatabase(ConfigEmu& emu){
	static string lastXmlRoute;
	dirutil dir;

	// 2. Carga/Verificación de la base de datos
    //if (mameDatabase.empty() && !emu.mame_roms_xml.empty())  {
	if (!emu.mame_roms_xml.empty() && lastXmlRoute != emu.mame_roms_xml)  {
		std::string mame_xml_path = dirutil::getPathPrefix(emu.mame_roms_xml);
		LOG_DEBUG("Limpiando cache de xml");
		lastXmlRoute = emu.mame_roms_xml;
		// En vez de mameDatabase.clear():
		std::map<std::string, GameData>().swap(mameDatabase);

		LOG_DEBUG("Cargando xml custom %s\n", mame_xml_path.c_str());
		//Buscamos primero con el xml custom
		parse_mame_names(mame_xml_path, mameDatabase);

		bool xmlFromMame = false;
		if (mameDatabase.size() == 0){
			// Buscamos los juegos con la etiqueta por defecto <game> del xml oficial de MAME
			// Version usada en mame 2003+
			LOG_DEBUG("Cargando xml mame 2003+");
		    parse_mame_xml(mame_xml_path, mameDatabase);
			xmlFromMame = true;
		}

		if (mameDatabase.size() == 0){
			// Si no lo hemos encontrado con la llamada anterior, es que juegos estan con la 
			// etiqueta <machine> del xml oficial de MAME.
			// Versiones nuevas de mame
			LOG_DEBUG("Cargando xml mame new");
			parse_mame_xml(mame_xml_path, mameDatabase, "machine");
			xmlFromMame = true;
		}

		LOG_DEBUG("Xml cargado con %d elementos\n", mameDatabase.size());
		if (mameDatabase.size() > 0 && xmlFromMame && mame_xml_path.find("merged_") == string::npos){
			//Solo recreamos si hemos obtenido el xml de las versiones oficiales de mame, ya que tienen demasiados tags a recorrer
			std::string newMameXmlPath = dirutil::getPathPrefix("merged_" + dir.getFileNameNoExt(mame_xml_path) + ".xml", dir.getFolder(mame_xml_path)) ;
			LOG_DEBUG("Guardando xml reducido %s\n", newMameXmlPath.c_str());
			write_mame_xml(newMameXmlPath, mameDatabase);
		}
    } else if (emu.mame_roms_xml.empty() && mameDatabase.size() > 0){
		// En vez de mameDatabase.clear():
		std::map<std::string, GameData>().swap(mameDatabase);
		lastXmlRoute = "";
	}
}

/**
 * 
*/
void ListMenu::filesToList(vector<unique_ptr<FileProps>> &files, ConfigEmu emu) {
	LOG_DEBUG("Generando lista a partir de los ficheros\n");
    this->clear();
	LOG_DEBUG("Lista de juegos anterior limpiada\n");
    dirutil dir;
    //Determinar el sistema
    vector<string> v = Constant::splitChar(emu.system, '_');
    int system = (v.size() > 0) ? Constant::strToTipo<int>(v.at(0)) : 0;
	//Cargamos las descripciones para roms de MAME, FBNeo, ...
	LOG_DEBUG("Cargando mame bdd\n");
	loadMameDatabase(emu);
    // Ahora comprobamos si el mapa tiene datos, independientemente de cuando se cargo
    bool hasMameData = !mameDatabase.empty();
	bool foundInMame = false;
	//Reservamos espacio y limpiamos el filtro
    listGames.reserve(files.size());
	gameDataFields.clear();
	LOG_DEBUG("Previous filters cleared and game list reserved \n");

	for (auto it = files.cbegin(); it != files.cend(); ++it) {
        const auto& file = *it;
		foundInMame = false;
		GameFile* gFile = new GameFile();

        gFile->systemid = system;
        gFile->longFileName = file->filename;

        const string fileNameNoExt = dir.getFileNameNoExt(file->filename);
		//LOG_DEBUG("buscando %s\n", fileNameNoExt.c_str());

        if (hasMameData) {
            // Buscamos en el mapa persistente
            std::map<std::string, GameData>::iterator it = mameDatabase.find(fileNameNoExt); 
            if (it != mameDatabase.end()) {
                gFile->gameData = &it->second; // Apuntamos a la memoria del mapa
                gFile->gameTitle = it->second.description;
                foundInMame = true;
				//LOG_DEBUG("Encontrada descripcion: %s \n", gFile->gameTitle.c_str());

				/** Podria parecer ineficiente meter todos los campos en un vector aunque esten duplicados,
				  * pero resulta que es mas rapido asi, y ordenar al final, eliminando duplicados,
				  * que usando un set o un map */
 				gameDataFields.manufacturers.push_back(it->second.manufacturer);
				gameDataFields.years.push_back(Constant::TipoToStr(it->second.year));

				std::string system = extractSystem(it->second.sourcefile);
				if (!system.empty())
					gameDataFields.systems.push_back(system);
            } 
			//else {
				//LOG_DEBUG("[MAME]NODESC: %s\n", fileNameNoExt.c_str());
			//}
        }

		if (!foundInMame) {
            gFile->gameTitle = fileNameNoExt;
        }
		
		// precalculo de clave para ordenar
		gFile->sortKey = gFile->gameTitle;
		Constant::lowerCase(&gFile->sortKey); 
        listGames.push_back(std::unique_ptr<GameFile>(gFile));
    }

	LOG_DEBUG("Files added");
	sortFilters();
	/*for (auto it = gameDataFields.systems.cbegin(); it != gameDataFields.systems.cend(); ++it) {
		const auto& s = *it;
		OutputDebugStringA(s.c_str());
		OutputDebugStringA("\n");
	}*/

	//Reset the filter and add all elements to be shown
	resetFilter();
}

std::string ListMenu::extractSystem(const std::string& sourceFile) {
    std::size_t posSlash = sourceFile.find('/');
	if (posSlash != std::string::npos) {
        std::size_t posCapcom = sourceFile.find("capcom");
        if (posCapcom != std::string::npos)
            return std::string(sourceFile, posSlash + 3, 4);
        else
            return std::string(sourceFile, 0, posSlash);
    }
    
	std::size_t posDot = sourceFile.find('.');
    if (posDot != std::string::npos)
        return std::string(sourceFile, 0, posDot);

    return sourceFile;
}

/**
* 
*/
void ListMenu::resetIndexPos(){
    this->listSize = this->filteredGames.size();
    this->maxLines = this->getScreenNumLines();
    /*To go to the bottom of the list*/
    //this->endPos = getListGames()->size();
    //this->iniPos = (int)getListGames()->size() >= this->maxLines ? getListGames()->size() - this->maxLines : 0;
    //this->curPos = this->endPos - 1;
    /*To go to the init of the list*/
    this->iniPos = 0;
    this->curPos = 0;
    this->endPos = (int)this->filteredGames.size() > this->maxLines ? this->maxLines : this->filteredGames.size();
    this->pixelShift = 0;
    this->lastSel = -1;
}

void ListMenu::nextPos(){
    if (this->curPos < this->listSize - 1){
        this->curPos++;
        int posCursorInScreen = this->curPos - this->iniPos;

        if (posCursorInScreen > this->maxLines - 1){
            this->iniPos++;
            this->endPos++;
        }
        this->pixelShift = 0;
        this->lastSel = -1;
    }
}

void ListMenu::prevPos(){
    if (this->curPos > 0){
        this->curPos--;
        if (this->curPos < this->iniPos && this->curPos >= 0){
            this->iniPos--;
            this->endPos--;
        }
        this->pixelShift = 0;
        this->lastSel = -1;
    }
}

void ListMenu::nextPage(){
    for (int i=0; i < this->maxLines -1; i++){
        nextPos();
    }
}

void ListMenu::prevPage(){
    for (int i=0; i < this->maxLines -1; i++){
        prevPos();
    }
}