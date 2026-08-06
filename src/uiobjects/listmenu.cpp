#include "listmenu.h"

#include <io/dirutil.h>
#include <beans/structures.h>
#include <font/fonts.h>
#include <gfx/SDL_gfxPrimitives.h>
#include <gfx/gfx_utils.h>
#include <io/cfgloader.h>
#include <ostream>
#ifdef _XBOX
	#include <xtl.h> // Cabecera obligatoria del XDK de Xbox
#endif

#include <unordered_set>
#include <cstring>

namespace {
/* Hash y comparador para usar `const char*` como clave en unordered_set,
 * comparando por contenido y no por puntero.  Permite deduplicar campos
 * `char[]` de GameData (manufacturer, sourcefile) sin construir std::string
 * temporales en cada iteracion. */
struct CStrHash {
    std::size_t operator()(const char* s) const {
        std::size_t h = 5381; // djb2
        while (*s) h = ((h << 5) + h) + (unsigned char)*s++;
        return h;
    }
};
struct CStrEqual {
    bool operator()(const char* a, const char* b) const {
        return std::strcmp(a, b) == 0;
    }
};
}

SDL_Surface* ListMenu::imgText;

void ListMenu::clearSelectedText(){
    if (imgText != NULL){
		SDL_FreeSurface(imgText);
        imgText = NULL;
    }
}

ListMenu::ListMenu(int screenw, int screenh){
	face_h_big = Fonts::getLineSkip(Fonts::FONTBIG);
	face_h_small = Fonts::getLineSkip(Fonts::FONTSMALL);

	iniPos = 0;
    endPos = 0;
    curPos = 0;
    listSize = 0;
    maxLines = 0;
    marginX = (int)floor((double)(screenw / 100));
	marginY = face_h_big * 2;
    lastSel = -1;
    pixelShift = 0;
    updateAssets = false;
    animateBkg = true;
	showBottomInfo = true;
    setObjectType(GUILISTBOX);
    setLayout(LAYSIMPLE, screenw, screenh);
    //set_trans_blender(255, 255, 255, 190);
	selecAlphaRec = NULL;
	navPath = NULL;
}

ListMenu::~ListMenu(){
	LOG_DEBUG("Deleting ListMenu...");
	//Respetar el orden de limpiado en la destruccion porque sino hay problemas
	filteredGames.clear();
	listGames.clear();
	if (navPath) SDL_FreeSurface(navPath);
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
    return face_h_big != 0 ? (int)std::floor((double)getH() / face_h_big) : 0;
}

/**
    * 
    */
void ListMenu::setLayout(int layout, int screenw, int screenh){
    clearSelectedText();
	this->marginY = face_h_big * 2;

    if (layout == LAYBOXES){
        this->setX(0);
        this->setY(marginY);
        this->setW(screenw / 2);
        this->setH(screenh - this->getY());
        this->centerText = false;
        this->layout = layout;

		if (showBottomInfo){
			this->setH(this->getH() - face_h_big * 2);
		}
	
	} else {
        this->setX(marginX);
        this->setY(marginY);
        this->setW(screenw - marginX);
        this->setH(screenh - this->getY());
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
	if (!listGames.empty()){
		filteredGames.reserve(listGames.size());
		for (auto it = listGames.cbegin(); it != listGames.cend(); ++it) {
			filteredGames.push_back(it->get());
		}
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
	LOG_DEBUG("Sorting the list of games");
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
	const int marginTextIcon = face_h_big;
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
	srcRect.h = face_h_big;
	//Para controlar el tiempo de scrolling del texto
	static uint32_t lastTick = 0;
	//To scroll one letter in one second. We use the face_h because the width of 
    //a letter is not fixed.
    const float pixelsScrollFps = max(ceil(face_h_big / (float)textFps), 1.0f);
	const SDL_Color colorTextSel = haveFocus ? Constant::colors[clBlack].sdlColor : Constant::colors[clDarkGray].sdlColor;
	const SDL_Color colorTextNotSel = haveFocus ? Constant::colors[clWhite].sdlColor : Constant::colors[clDarkGray].sdlColor;

	//Creamos el rectangulo de elemento seleccionado translucido
	SDL_Rect rectElem = {0, 0, this->getW() - 2 * marginX, face_h_big};
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

	//Dibujamos la ruta relativa que estamos explorando
	drawNavBar(video_page, colorTextNotSel, fontMenu, face_h_big);

	//Dibujamos el texto
    for (int i=this->iniPos; i < this->endPos; i++){
        auto& game = filteredGames.at(i);
        const int screenPos = i - this->iniPos;
        const int fontHeightRect = screenPos * face_h_big;
        SDL_Rect dstRectIcon = {this->getX() + marginX, this->getY() + fontHeightRect + 2, 0, 0};
        SDL_Rect dstRectWithMargin = {dstRectIcon.x + marginTextIcon, dstRectIcon.y - 2, 0, 0};
		const string line = game->gameTitle.empty() ? game->longFileName : game->gameTitle;

        // Color distinto si esta seleccionado
        const SDL_Color lineTextColor = i == this->curPos ? colorTextSel : colorTextNotSel;
		// Establecemos el inicio de la superficie del texto que se mostara con scroll
		srcRect.x = i == this->curPos ? (Sint16)pixelShift : 0;

		//Drawing a faded background selection rectangle
        if (i == this->curPos){
			SDL_Rect rectElemBlit = {this->getX() + marginX, this->getY() + fontHeightRect, rectElem.w, rectElem.h};
			if (haveFocus && selecAlphaRec){
				SDL_BlitSurface(selecAlphaRec, NULL, video_page, &rectElemBlit);
			} else {
				rect(video_page, rectElemBlit.x, rectElemBlit.y, rectElemBlit.x + rectElemBlit.w - 1, rectElemBlit.y + rectElemBlit.h - 1, Constant::colors[clBkgMenu].sdlColor);
			}
        }

        if (game->cache == NULL) {
            // Cache no existe -> renderizar
			#ifdef _XBOX 
			game->cache = Fonts::renderUtf8Solid(fontMenu, line.c_str(), lineTextColor);
			#else
			game->cache = Fonts::renderUtf8Blended(fontMenu, line.c_str(), lineTextColor);
			#endif
			//Huge speedup if the background is static
			// 1. Crear superficie optimizada de 8 bits con Anti-aliasing
			//game->cache = TTF_RenderUTF8_Shaded(fontMenu, line.c_str(), lineTextColor, Constant::colors[clBlack].sdlColor);
			// 2. Hacer que el fondo negro (índice 0) sea completamente transparente
			// SDL_SRCCOLORKEY activa la transparencia por color en el volcado
			//SDL_SetColorKey(game->cache, SDL_SRCCOLORKEY, 0); 
        } 

		SDL_BlitSurface(game->cache, &srcRect, video_page, &dstRectWithMargin);

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
		drawIconListElem(video_page, game, dstRectIcon);
    }
}

void ListMenu::drawNavBar(SDL_Surface *video_page, const SDL_Color& txtColor,
                          TTF_Font *fontMenu, const int& face_h)
{
    // --- 1. Construir txtNav ---
    std::string txtNav;
    if (!listZipped.file.empty()) {
        txtNav = listZipped.file;
        for (std::size_t i = 0; i < listZipped.pathInZip.size(); ++i)
            appendSegment(txtNav, listZipped.pathInZip[i]);
    } else {
        for (std::size_t i = 0; i < listDir.relativePath.size(); ++i)
            appendSegment(txtNav, listDir.relativePath[i]);
    }
	
    // --- 2. Salida temprana si no hay ruta ---
    if (txtNav.empty()) {
        if (navPath)         { SDL_FreeSurface(navPath);         navPath         = NULL; }
        lastTxtNav = txtNav;
        return;
    }

	SDL_Rect rectNavPath = {this->getX(), this->marginY, this->getW() - 1, face_h};
	
	// --- 3. Regenerar superficie de texto si cambió ---
    if (txtNav != lastTxtNav) {
        lastTxtNav = txtNav;

        const int prevStyle = TTF_GetFontStyle(fontMenu);
        TTF_SetFontStyle(fontMenu, TTF_STYLE_ITALIC | TTF_STYLE_BOLD);

        if (navPath) {
			SDL_FreeSurface(navPath);
			navPath = NULL;
		}
#ifdef _XBOX
        SDL_Surface *txtNavPath = Fonts::renderUtf8Solid  (fontMenu, txtNav.c_str(), Constant::colors[clBkgMenu].sdlColor);
#else
        SDL_Surface *txtNavPath = Fonts::renderUtf8Blended(fontMenu, txtNav.c_str(), Constant::colors[clBkgMenu].sdlColor);
#endif
        TTF_SetFontStyle(fontMenu, prevStyle);

		//Creamos una superficie transparente de color negro
		Constant::createRectAlphaFilled(navPath, rectNavPath, video_page->format, clBG, true);

        if (!txtNavPath || !navPath) 
			return;

		fastline(navPath, 0, rectNavPath.h-1, rectNavPath.w, rectNavPath.h-1, Constant::colors[clMenuBars].sdlColor);
		
		//Y le dibujamos el texto renderizado anteriormente
		SDL_Rect rectSrc = {0, 0, txtNavPath->w, face_h};
		if (txtNavPath->w > rectNavPath.w){
			rectSrc.x = txtNavPath->w - rectNavPath.w;
			rectSrc.w = rectNavPath.w;
		} 

		SDL_BlitSurface(txtNavPath, &rectSrc, navPath, NULL);
		SDL_FreeSurface(txtNavPath);
    }
    SDL_BlitSurface(navPath, NULL, video_page, &rectNavPath);
}

void ListMenu::drawIconListElem(SDL_Surface *video_page, GameFile *game, SDL_Rect& dstRectIcon) {
	SDL_Surface *icon = NULL;

	// 1. Intentar obtener el icono de cartucho si aplica
	const unsigned int posCart = getCartForSystem(game->systemid);
	if (game->fileType == FT_CARTRIDGE) {
		if (Icons::getInstance().drawIconCart(video_page, &dstRectIcon, posCart))
			return; // Si se dibuja el cartucho, terminamos aqui
	}

	// 2. Si no es cartucho (o no tenia imagen), determinar el tipo de icono generico
	enumIco ico = page_white;
	if (game->fileType == FT_DIR) {
		ico = folder;
	} else if (game->fileType == FT_ZIP_LIST) {
		ico = page_white_zip;
	}

	// 3. Obtener y dibujar el icono generico aplicando el desplazamiento
	dstRectIcon.x -= 7;
	dstRectIcon.y -= 7;
    Icons::getInstance().drawIcon(video_page, &dstRectIcon, ico);
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
		case 11: 
			return cart_virtualboy;
		case 12:
			return cart_gba;
		case 19:
			return cart_32x;
		case 20:
			return cart_mcd;
		case 21:
			return cart_gg;
		case 25:
			return cart_neogeo_pocket;
		case 29:
			return cart_3do;
		case 31:
			return cart_pce;
		case 46:
			return cart_wonderswan;
		case 57:
			return cart_psx;
		case 75:
			return cart_mame;
		case 76:
			return cart_zx;
		case 105:
			return cart_supergrafx;
		case 113:
			return cart_msx;
		case 114:
			return cart_pce_cd;
		case 135:
			return cart_dos;
		case 290:
			return cart_doom;
		case 666:
			return cart_quake;
		default:
			return cart_default;
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
    // 1. Si los tipos son diferentes, ordenamos estrictamente por el valor del enum (0, luego 1, luego 2)
    if (a->fileType != b->fileType) {
        return a->fileType < b->fileType;
    }
    
    // 2. Si son del mismo tipo (ej. ambos son directorios), ordenamos alfabéticamente
    return a->sortKey < b->sortKey;
}

DWORD WINAPI HiloDestructor(LPVOID lpParam) {
    DatosDestruccion* datos = static_cast<DatosDestruccion*>(lpParam);
    
    if (datos != NULL) {
        // Ejecuta la limpieza costosa en este hilo secundario
        delete datos->vectorVacio; 
        
        // Limpiamos la estructura contenedora de parámetros
        delete datos;
    }
    return 0;
}

void ListMenu::loadMameDatabase(ConfigEmu& emu){
	static string lastXmlRoute;
	dirutil dir;

	uint32_t time = SDL_GetTicks();
	std::unordered_map<std::string, GameData> *vectorVacio = new std::unordered_map<std::string, GameData>();

	//Carga/Verificación de la base de datos
	if (!emu.mame_roms_xml.empty() && lastXmlRoute != emu.mame_roms_xml)  {
		std::string mame_xml_path = dirutil::getPathPrefix(emu.mame_roms_xml);
		lastXmlRoute = emu.mame_roms_xml;
		// En vez de mameDatabase.clear():
		mameDatabase.swap(*vectorVacio);
		LOG_DEBUG("mameDatabase cleared %d", SDL_GetTicks() - time);
		time = SDL_GetTicks();

		//Buscamos primero con el xml custom
		parse_mame_names(mame_xml_path, mameDatabase);
		LOG_DEBUG("Cargado xml custom %s - %d", mame_xml_path.c_str(), SDL_GetTicks() - time);
		time = SDL_GetTicks();

		bool xmlFromMame = false;
		if (mameDatabase.size() == 0){
			// Buscamos los juegos con la etiqueta por defecto <game> del xml oficial de MAME
			// Version usada en mame 2003+
		    parse_mame_xml(mame_xml_path, mameDatabase);
			xmlFromMame = true;
			LOG_DEBUG("Cargando xml mame 2003+ %d", SDL_GetTicks() - time);
			time = SDL_GetTicks();
		}

		if (mameDatabase.size() == 0){
			// Si no lo hemos encontrado con la llamada anterior, es que juegos estan con la 
			// etiqueta <machine> del xml oficial de MAME.
			// Versiones nuevas de mame
			parse_mame_xml(mame_xml_path, mameDatabase, "machine");
			xmlFromMame = true;
			LOG_DEBUG("Cargando xml mame new %d", SDL_GetTicks() - time);
			time = SDL_GetTicks();
		}

		LOG_DEBUG("Xml cargado con %d elementos\n", mameDatabase.size());
		if (mameDatabase.size() > 0 && xmlFromMame && mame_xml_path.find("merged_") == string::npos){
			//Solo recreamos si hemos obtenido el xml de las versiones oficiales de mame, ya que tienen demasiados tags a recorrer
			std::string newMameXmlPath = dirutil::getPathPrefix("merged_" + dir.getFileNameNoExt(mame_xml_path) + ".xml", dir.getFolder(mame_xml_path)) ;
			write_mame_xml(newMameXmlPath, mameDatabase);
			LOG_DEBUG("Guardando xml reducido %s - %d", newMameXmlPath.c_str(), SDL_GetTicks() - time);
			time = SDL_GetTicks();
		}
    } else if (emu.mame_roms_xml.empty() && mameDatabase.size() > 0){
		// En vez de mameDatabase.clear():
		mameDatabase.swap(*vectorVacio);
		lastXmlRoute = "";
	}

	if (!vectorVacio->empty()) {
		// Reservamos los datos que se enviarán al hilo
		DatosDestruccion* datos = new DatosDestruccion();
		datos->vectorVacio = vectorVacio;

		// Creamos el hilo en segundo plano
		HANDLE hThread = CreateThread(
			NULL,               // Atributos de seguridad por defecto
			0,                  // Tamaño de pila por defecto
			HiloDestructor,     // Función que ejecutará el hilo
			datos,              // Parámetro enviado a la función
			CREATE_SUSPENDED,   // Flags de creación (0 de inmediato)
			NULL                // No necesitamos guardar el ID del hilo
		);

		if (hThread != NULL) {
			// OPTIONAL / RECOMENDADO EN XBOX 360:
			// El procesador de Xbox 360 tiene 3 núcleos físicos (6 hilos de hardware).
			// Mandamos esta tarea pesada al Núcleo 2 (Hilo de hardware 4) para no perturbar el juego.
			#ifdef _XBOX
			XSetThreadProcessor(hThread, IO_THREAD);
			#endif
			ResumeThread(hThread);
			// Cerramos el handle del hilo inmediatamente. 
			// Esto NO destruye el hilo, solo le dice a la Xbox que libere sus recursos 
			// automáticamente cuando 'HiloDestructor' termine de ejecutarse (evita memory leaks).
			//CloseHandle(hThread);
		} else {
			// Fallback de seguridad: si el hilo falla al crearse, limpiamos en el hilo principal
			delete vectorVacio;
			delete datos;
		}
	}
}

/**
 * 
*/
void ListMenu::filesToList(vector<unique_ptr<FileProps>> &files, ConfigEmu emu) {
	LOG_DEBUG("Generando lista a partir de los ficheros\n");
	uint32_t time = SDL_GetTicks();
    this->clear();
	LOG_DEBUG("Lista de juegos anterior limpiada %d", SDL_GetTicks() - time);
	time = SDL_GetTicks();
    dirutil dir;
	/* Deduplicacion eficiente:
	 *  - Manufacturer y sourcefile son char[] dentro de GameData (stable),
	 *    asi que guardamos punteros directos con hash/equal por contenido
	 *    y evitamos construir std::string temporales por juego.
	 *  - `extractSystem` se difiere a despues del bucle: solo se llama una
	 *    vez por cada sourcefile unico (~30) en lugar de N veces. */
	std::tr1::unordered_set<const char*, CStrHash, CStrEqual> uniqueManufacturers(256);
	std::tr1::unordered_set<uint16_t>                         uniqueYears(128);
	std::tr1::unordered_set<const char*, CStrHash, CStrEqual> uniqueSourcefiles(64);

	//Determinar el sistema
    vector<string> v = Constant::splitChar(emu.system, '_');
    int system = (v.size() > 0) ? Constant::strToTipo<int>(v.at(0)) : 0;
	//Cargamos las descripciones para roms de MAME, FBNeo, ...
	loadMameDatabase(emu);
	LOG_DEBUG("Cargado mame bdd %d", SDL_GetTicks() - time);
	time = SDL_GetTicks();

    // Ahora comprobamos si el mapa tiene datos, independientemente de cuando se cargo
    bool hasMameData = !mameDatabase.empty();
	bool foundInMame = false;
	//Reservamos espacio y limpiamos el filtro
    listGames.reserve(files.size());
	LOG_DEBUG("Game list reserved %d", SDL_GetTicks() - time);
	time = SDL_GetTicks();
	gameDataFields.clear();
	LOG_DEBUG("Previous filters cleared %d", SDL_GetTicks() - time);
	time = SDL_GetTicks();

	// Caché local para evitar llamar a cbegin()/cend() repetidamente en el bucle
	const auto endIt = files.cend(); 

	for (auto it = files.cbegin(); it != endIt; ++it) {
		const auto& file = *it;
    
		GameFile* gFile = new GameFile();
		gFile->systemid = system;
    
		// Intercambio instantáneo de memoria en 0ms
		gFile->longFileName.swap(file->filename); 
    
		// 1. Calculamos el nombre sin extensión una sola vez
		const string fileNameNoExt = dir.getFileNameNoExt(gFile->longFileName);
    
		// Variable local para el control de MAME en esta iteración concreta
		bool gFileFoundInMame = false;

		if (hasMameData) {
			// 2. Búsqueda en el árbol del mapa (std::map)
			auto itMame = mameDatabase.find(fileNameNoExt); 
			if (itMame != mameDatabase.end()) {
				const GameData& data = itMame->second; // Referencia directa para evitar escribir 'itMame->second'
            
				gFile->gameData = &data; 
				gFile->gameTitle = data.description; // Copia el título real de MAME
				gFileFoundInMame = true;

				uniqueManufacturers.insert(data.manufacturer); // const char* directo, 0 allocs
				uniqueYears.insert(data.year);
				uniqueSourcefiles.insert(data.sourcefile);     // dedup en crudo; extractSystem se aplica fuera
			} 
		}
    
		if (!gFileFoundInMame) {
			gFile->gameTitle = fileNameNoExt;
		}

		const std::string& longName = gFile->longFileName;
		if (!longName.empty() && longName[0] == '@') {
			gFile->fileType = FT_ZIP_LIST;
		} else {
			gFile->fileType = (file->filetype == TIPODIRECTORIO) ? FT_DIR : FT_CARTRIDGE;
		}

		// precalculo de clave para ordenar
		gFile->sortKey = gFile->gameTitle;
		Constant::lowerCase(&gFile->sortKey); 
		listGames.emplace_back(gFile);
	}

	LOG_DEBUG("%d Files added %d(ms)", listGames.size(), SDL_GetTicks() - time);
	time = SDL_GetTicks();

	// Volcamos los elementos ÚNICOS a los vectores de la estructura.
	// Una sola allocacion de std::string por valor unico (no por juego).
	gameDataFields.manufacturers.clear();
	gameDataFields.manufacturers.reserve(uniqueManufacturers.size());
	for (auto it = uniqueManufacturers.begin(); it != uniqueManufacturers.end(); ++it) {
		gameDataFields.manufacturers.emplace_back(*it); // const char* -> std::string, 1 vez por unico
	}

	// Sistemas: aplicamos extractSystem solo a sourcefiles unicos (~30 llamadas en vez de N).
	// Volvemos a deduplicar el resultado por si dos sourcefiles distintos colapsan al mismo sistema.
	gameDataFields.systems.clear();
	gameDataFields.systems.reserve(uniqueSourcefiles.size());
	std::tr1::unordered_set<std::string> uniqueSystemsOut(uniqueSourcefiles.size());
	for (auto it = uniqueSourcefiles.begin(); it != uniqueSourcefiles.end(); ++it) {
		std::string sys = extractSystem(std::string(*it));
		if (uniqueSystemsOut.insert(sys).second) {
			gameDataFields.systems.push_back(sys);
		}
	}

	//Volcado manual convirtiendo cada número a string
	gameDataFields.years.clear();
	gameDataFields.years.reserve(uniqueYears.size());
	for (auto it = uniqueYears.begin(); it != uniqueYears.end(); ++it) {
		gameDataFields.years.emplace_back(Constant::TipoToStr(*it));
	}

	sortFilters();
	applyFilter();
	resetIndexPos();

	/*for (auto it = gameDataFields.systems.cbegin(); it != gameDataFields.systems.cend(); ++it) {
		const auto& s = *it;
		OutputDebugStringA(s.c_str());
		OutputDebugStringA("\n");
	}*/

	LOG_DEBUG("Files sorted and filter reset %d", SDL_GetTicks() - time);
	time = SDL_GetTicks();
}

void ListMenu::zippedToList(int system) {
	dirutil dir;

	for (std::size_t i = 0; i < listZipped.entries.size(); ++i)
    {
        const ZipEntry& e = listZipped.entries[i];
        //LOG_DEBUG("%s, %s, %s, %s", (e.isDir ? "[DIR]" : "[FIC]"), e.name.c_str(), 
		//	(e.isDir ? "-" : std::to_string((long long)e.uncompressedSize).c_str()),
        //    (e.isDir ? "-" : std::to_string((long long)e.compressedSize).c_str()));

		GameFile* gFile = new GameFile();
        gFile->systemid = system;
		gFile->fileType = e.isDir ? FT_DIR : FT_CARTRIDGE;
        gFile->longFileName = e.name;
        const string fileNameNoExt = dir.getFileNameNoExt(e.name);
        gFile->gameTitle = fileNameNoExt;
        listGames.push_back(std::unique_ptr<GameFile>(gFile));
    }
	//Reset the filter and add all elements to be shown
	resetFilter();
}

std::string ListMenu::extractSystem(const std::string& sourceFile) {
    // 1. Buscamos el separador '/' de forma directa
    const std::size_t posSlash = sourceFile.find('/');
    if (posSlash != std::string::npos) {
        // 2. Buscamos "capcom" de forma óptima
        if (sourceFile.find("capcom") != std::string::npos) {
            // Devuelve subcadena desde posSlash + 3 con una longitud de 4 caracteres
            // .substr() activa de forma nativa la optimización RVO en VS2010
            return sourceFile.substr(posSlash + 3, 4);
        }
        // Si no es capcom, devolvemos todo lo anterior al slash
        return sourceFile.substr(0, posSlash);
    }
    
    // 3. Si no hay slash, buscamos el punto de la extensión
    const std::size_t posDot = sourceFile.find('.');
    if (posDot != std::string::npos) {
        return sourceFile.substr(0, posDot);
    }

    // 4. Si no cumple nada, devolvemos una copia limpia de la cadena completa
    return sourceFile;
}

/**
* 
*/
void ListMenu::resetIndexPos(){
    this->listSize = this->filteredGames.size();
    this->maxLines = this->getScreenNumLines();
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

void ListMenu::resizeMarginTop(int addedMargin, int screenH){
	//Cambiamos el tamanyo del listado para poder mostrar la ruta relativa de directorios
	this->setY(this->marginY + addedMargin);
	this->setH(screenH - this->getY());
	if (showBottomInfo){
		this->setH(this->getH() - face_h_big * 2);
	}
	this->maxLines = this->getScreenNumLines();
	this->endPos = (int)this->filteredGames.size() > this->maxLines ? this->iniPos + this->maxLines : this->filteredGames.size();
}