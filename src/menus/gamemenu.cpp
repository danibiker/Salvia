#include "gamemenu.h"

#include <gfx/SDL_gfxPrimitives.h>
#include <gfx/SDL_rotozoom.h>
#include <gfx/gfx_utils.h>
#include <SDL_Image.h>
#include <io/dirutil.h>
#include <beans/structures.h>
#include <so/launcher.h>
#include <libretro/libretro.h>
#include <utils/langmanager.h>
#include <menus/mameparser.h>
#include <io/keyboard.h>
#include "unzip/unziptool.h"
#include "rhash/md5.h" // To generate a filename hash

/* Definida en salvia.cpp devuelve los descriptores de memoria que el core
 * envio via RETRO_ENVIRONMENT_SET_MEMORY_MAPS (ej. HRAM en Game Boy). */
extern const struct retro_memory_descriptor* get_core_memory_descriptors(unsigned* out_count);

GameMenu::GameMenu(CfgLoader *cfgLoader) : m_csInited(false)
{
    status = EMU_MENU;
	lastStatus = EMU_MENU;
	onscreenKeyboard = false;
	romLoaded = false;
	gameTicks.ticks = 0;

	this->cfgLoader = cfgLoader;
	this->initEngine(cfgLoader);

	selectedFsImage = 0;
	fsImage.setX(0);
	fsImage.setY(0);
	fsImage.setW(overlay->w);
	fsImage.setH(overlay->h);

	/* Lock + worker para la carga asincrona del panel de assets del menu. */
	InitializeCriticalSection(&m_csSnap);
	InitializeCriticalSection(&m_csBox2d);
	InitializeCriticalSection(&m_csSnaptit);
	InitializeCriticalSection(&m_csSynopsis);
	m_csInited = true;
	m_menuAssetLoader.start(this);

	face_h_big = Fonts::getLineSkip(Fonts::FONTBIG);
	face_h_small = Fonts::getLineSkip(Fonts::FONTSMALL);

	std::string initMsg = "Loading " + Constant::getAppExecutable() + "...";
	Fonts::drawTextCentTransparent(overlay, Fonts::getFont(Fonts::FONTBIG), initMsg.c_str(), 0, -face_h_big / 2, true, true, Constant::colors[clWhite].sdlColor, 0);
	salviaFlip(gameScreen);

	
	int pixelDato = Fonts::getSize(Fonts::FONTSMALL, "FPS: 888.8");

	rectFps.x = this->overlay->w - pixelDato - 3;
	rectFps.y = 2 * face_h_small;
	rectFps.w = this->overlay->w - rectFps.x;
	rectFps.h = face_h_small;
	bkgTextFps = Constant::colors[clBlack].color;
	uBkgColor = Constant::colors[clBackground].color;

	bg_image.setW(this->overlay->w);
	bg_image.setH(this->overlay->h);
	bg_image.fillGaps = true;
	bg_image.darkShift = 150;
	bg_image.bestFit = true;

	if (!joystick->init_all_joysticks()){
		configButtonsJOY();
	}

	configMenus = new GestorMenus(overlay->w, overlay->h);
	configMenus->inicializar(cfgLoader, joystick);
	// En la clase Config o GameMenu
	selectScalerMode(FULLSCREEN);

	this->current_scaler_mode = &getCfgLoader()->configMain[cfg::scaleMode].getIntRef();
	this->current_ratio = &getCfgLoader()->configMain[cfg::aspectRatio].getIntRef();
	this->current_sync = &getCfgLoader()->configMain[cfg::syncMode].getIntRef();
	this->current_integer_scale = &getCfgLoader()->configMain[cfg::integerScale].getBoolRef();
	this->current_integer_scale_type = &getCfgLoader()->configMain[cfg::scaleIntMode].getIntRef();
	this->current_shader = &getCfgLoader()->configMain[cfg::shaderMode].getIntRef();
	this->mustUpdateFps = &getCfgLoader()->configMain[cfg::showFps].getBoolRef();
	this->current_fast_forward = false;
	processConfigChanges();

	fpsSurface = NULL; 
	cpuSurface = NULL;
	memSurface = NULL;
	bg_screenshot = NULL;
	filterAlphaRec = NULL;
	infoBtnSrf = NULL;
	lastFpsUpdate = 0;
	lastMemUpdate = 0;
	cargarSystemAchievementTranslation(Constant::getAppDir() + ROUTE_ACHIEVEMENT_TRANSLATIONS);
	initAchievements();
	Launcher::initDrives();
	joystick->setInfoButtons();
};

GameMenu::~GameMenu(){
	LOG_DEBUG("Deleting GameMenu...");

	/* Parar el worker ANTES de liberar nada que el pueda tocar
	 * (menuImages/menuTextAreas se destruyen al final). */
	m_menuAssetLoader.stop();
	if (m_csInited) {
		DeleteCriticalSection(&m_csSnap);
		DeleteCriticalSection(&m_csBox2d);
		DeleteCriticalSection(&m_csSnaptit);
		DeleteCriticalSection(&m_csSynopsis);
		m_csInited = false;
	}

	delete configMenus;

	if (fpsSurface) SDL_FreeSurface(fpsSurface);
	if (cpuSurface) SDL_FreeSurface(cpuSurface);
	if (memSurface) SDL_FreeSurface(memSurface);
	if (filterAlphaRec) SDL_FreeSurface(filterAlphaRec);
	if (infoBtnSrf) SDL_FreeSurface(infoBtnSrf);

	for (int i=0; i < (int)messages.size(); i++) {
		SDL_FreeSurface(messages[i].cache);
	}
		
	if (bg_screenshot) SDL_FreeSurface(bg_screenshot);
	clearLastAchievementArea();

	#if !defined(_XBOX) && !defined(SALVIA_GPU_VIDEO)
		hqxClose();
	#endif

	Achievements::instance()->shutdown();
}

/**
 * 
 */
void GameMenu::createMenuImages(ListMenu &listMenu){
    /* Bloquea el worker mientras reconstruimos los maps: createMenuImages
     * se llama al cambiar de emulador, y el worker podria tener una
     * peticion en vuelo del emulador anterior accediendo a menuImages[X]
     * justo cuando hacemos clear()/insert(). */
    if (m_csInited) {
        EnterCriticalSection(&m_csSnap);
        EnterCriticalSection(&m_csBox2d);
        EnterCriticalSection(&m_csSnaptit);
        EnterCriticalSection(&m_csSynopsis);
    }

	title_image.setY(5);
	title_image.setH(listMenu.marginY - 10);
	title_image.setW(overlay->w);
	title_image.keepAlpha = true;

	LOG_DEBUG("El title tiene una altura de %d", title_image.getH());

    /** snap */
    Image imageSnap;
    const int snapW = overlay->w / 2;
    const int snapH = listMenu.getH() / 2;
    //const int snapOffset = overlay->w / 10;
    const int snapOffset = 5;
	const int box2dH = (int)(listMenu.getH() / 3.5f);
    const int box2dW = overlay->w / 8;
    const int snapTitH = listMenu.getH() / 4;
    const int snapTitW = overlay->w / 6;

    menuImages.clear();
    menuTextAreas.clear();

	/** snap */
    imageSnap.setX(overlay->w / 2 + box2dW);
    imageSnap.setY(listMenu.getY());
    imageSnap.setW(snapW - box2dW);
    imageSnap.setH(snapH);
    imageSnap.vAlign = ALIGN_TOP;
	imageSnap.drawShadow = true;
    menuImages.insert(make_pair(SNAP, imageSnap));
    menuImages[SNAP].m_objCS = &m_csSnap;

    /** Box2d */
    Image imageBox2d;
    
	imageBox2d.setX(overlay->w / 2 + (snapTitW-box2dW) / 2);
    imageBox2d.setY(overlay->h / 2 - box2dH);
    imageBox2d.setW(box2dW);
    imageBox2d.setH(box2dH);
	imageBox2d.drawShadow = true;
    menuImages.insert(make_pair(BOX2D, imageBox2d));
    menuImages[BOX2D].m_objCS = &m_csBox2d;

    /** snaptit*/
    Image imageSnaptit;
    imageSnaptit.setX(overlay->w / 2);
    imageSnaptit.setY(listMenu.getY());
    imageSnaptit.setW(snapTitW);
    imageSnaptit.setH(snapTitH);
	imageSnaptit.drawShadow = true;
    menuImages.insert(make_pair(SNAPTIT, imageSnaptit));
    menuImages[SNAPTIT].m_objCS = &m_csSnaptit;

    Image imageSnapFs(0, 0, overlay->w, overlay->h);
    menuImages.insert(make_pair(SNAPFS, imageSnapFs));
	
	
	const int sectionGap = 0;
	int posTextSectionY = listMenu.getH() / 2 + listMenu.getY() + sectionGap;

	TextArea textareaYear(overlay->w / 2, posTextSectionY, overlay->w / 2, face_h_big);
	textareaYear.setFontType(Fonts::FONTBIG);
    textareaYear.marginX = (int)floor((double)overlay->w / 100);
    menuTextAreas.insert(make_pair(YEAR, textareaYear));

	posTextSectionY += face_h_big;
	TextArea textareaManufacturer(overlay->w / 2, posTextSectionY, overlay->w / 2, face_h_big);
	textareaManufacturer.setFontType(Fonts::FONTBIG);
	textareaManufacturer.marginX = textareaYear.marginX;
    menuTextAreas.insert(make_pair(MANUFACTURER, textareaManufacturer));

	posTextSectionY += face_h_big;
	TextArea textareaSystem(overlay->w / 2, posTextSectionY, overlay->w / 2, face_h_big);
	textareaSystem.setFontType(Fonts::FONTBIG);
    textareaSystem.marginX = textareaYear.marginX;
    menuTextAreas.insert(make_pair(SYSTEM, textareaSystem));

    posTextSectionY += face_h_big;
    TextArea textareaSyn(overlay->w / 2, posTextSectionY, overlay->w / 2, overlay->h - posTextSectionY);
	textareaSyn.setFontType(Fonts::FONTBIG);
    textareaSyn.marginX = (int)floor((double)overlay->w / 100);
    menuTextAreas.insert(make_pair(SYNOPSIS, textareaSyn));
    menuTextAreas[SYNOPSIS].m_objCS = &m_csSynopsis;

    if (m_csInited) {
        LeaveCriticalSection(&m_csSynopsis);
        LeaveCriticalSection(&m_csSnaptit);
        LeaveCriticalSection(&m_csBox2d);
        LeaveCriticalSection(&m_csSnap);
    }
}

bool GameMenu::someImageLoaded(){
	std::map<std::string, Image>::iterator it;
	bool hasImage = false;
	for (it = menuImages.begin(); it != menuImages.end() && !hasImage; ++it) {
		std::string clave = it->first;
		hasImage = it->second.hasImage();
	}
	return hasImage;
}

void GameMenu::findFirstImage(){
	int count = 0;
	if (sizeof(FS_IMAGES[0]) > 0){
		count = sizeof(FS_IMAGES) / sizeof(FS_IMAGES[0]);
	}
	const int initial = selectedFsImage;
	while (!menuImages[FS_IMAGES[selectedFsImage]].hasImage()){
		selectedFsImage = (selectedFsImage + 1) % count;
		if (selectedFsImage == initial)
			break;
	}
}

void GameMenu::nextImageLoaded(){
	bool hasImage = false;
	int count = 0;
	if (sizeof(FS_IMAGES[0]) > 0){
		count = sizeof(FS_IMAGES) / sizeof(FS_IMAGES[0]);
	}
	const int initial = selectedFsImage;
	do{
		selectedFsImage = (selectedFsImage + 1) % count;
		hasImage = menuImages[FS_IMAGES[selectedFsImage]].hasImage();
	} while (!hasImage && initial != selectedFsImage);
}

void GameMenu::prevImageLoaded(){
	bool hasImage = false;
	int count = 0;
	if (sizeof(FS_IMAGES[0]) > 0){
		count = sizeof(FS_IMAGES) / sizeof(FS_IMAGES[0]);
	}
	const int initial = selectedFsImage;

	do{
		selectedFsImage = selectedFsImage == 0 ? count - 1 : selectedFsImage - 1;
		hasImage = menuImages[FS_IMAGES[selectedFsImage]].hasImage();
	} while (!hasImage && initial != selectedFsImage);
}

/**
 * 
 */
void GameMenu::refreshScreen(ListMenu &listMenu){
	const ConfigEmu emu = *cfgLoader->getCfgEmu();
    //Drawing the emulator name
    TTF_Font *fontBig = Fonts::getFont(Fonts::FONTBIG);
    TTF_Font *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
    const int sepVertX = listMenu.getW();
    const int halfWidth = overlay->w / 2;
	bool debug = true;
	const SDL_Color &textColor = Constant::colors[clWhite].sdlColor;
	const SDL_Color &menuBars = Constant::colors[clMenuBars].sdlColor;

	if (getEmuStatus() == EMU_MENU_IMAGE_VIEWER){
		const std::string path = menuImages[FS_IMAGES[selectedFsImage]].getFilepath();
		if (path != fsImage.getFilepath()){
			fsImage.cloneSurface(menuImages[FS_IMAGES[selectedFsImage]].getImg(), path, overlay->format);
		}
		fsImage.printImage(overlay);
	} else if (listMenu.getNumGames() > (std::size_t)listMenu.curPos){
		//Drawing the rest of list and images
        auto game = listMenu.filteredGames.at(listMenu.curPos);
		if (!game->longFileName.empty()){
            if (listMenu.layout == LAYBOXES) {
				//Draw the title. It can be an image or a text
				drawTitle(listMenu, fontBig);
				//Draw a status bar with the time, connection status and mouse availability 
				drawStatusBar(listMenu);
				//Draw the scrapping process text
				showScrapProcess(listMenu);
				//Horizontal separation line
				fastline(this->overlay, listMenu.marginX, listMenu.marginY - 1 , overlay->w - listMenu.marginX, listMenu.marginY - 1, menuBars);
				//Vertical separation line
                fastline(this->overlay, sepVertX, listMenu.marginY , sepVertX, listMenu.getH() + listMenu.marginY - 1, menuBars);
				//Drawing all the menu entries
                listMenu.draw(this->overlay, getEmuStatus() != EMU_MENU_FILTER);

				if (getEmuStatus() == EMU_MENU_FILTER){
					drawFilters(listMenu);
				} else {
					drawSelectedGameAssets(listMenu, game);
				}

				if (listMenu.showBottomInfo){
					const int posYBottom = listMenu.getY() + listMenu.getH();
					fastline(this->overlay, 0, posYBottom , overlay->w - 1, posYBottom, menuBars);
					SDL_Rect rect = {0, posYBottom + 1, overlay->w, overlay->h - posYBottom};
					drawInfoButtons(rect);
				}

            } else if (listMenu.layout == LAYSIMPLE) {
                if (listMenu.updateAssets){
                    //Snapshot picture
                    menuImages[SNAPFS].loadImageFromGame(dirutil::getPathPrefix(emu.assets) + string(Constant::tempFileSep)
                        + "snap" + string(Constant::tempFileSep), *game, ".png");
                }
                menuImages[SNAPFS].printImage(this->overlay);
                //Draw the menu element after the image
                Fonts::drawTextCent(overlay, fontBig, emu.name.c_str(), 
					halfWidth, face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
					true, false, textColor, 0);

                fastline(this->overlay, listMenu.marginX, listMenu.marginY - 1, listMenu.getW(), listMenu.marginY - 1, textColor);
                listMenu.draw(this->overlay);

            } else if (listMenu.layout == LAYTEXT) {

				Fonts::drawTextCent(overlay, fontBig, emu.name.c_str(), 
					halfWidth, face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
					true, false, textColor, 0);

                fastline(this->overlay, listMenu.marginX, listMenu.marginY - 1, listMenu.getW(), listMenu.marginY - 1, textColor);
                listMenu.draw(this->overlay);
            }
        }
	} else if (listMenu.getNumGames() == 0 && emu.generalConfig){
		configMenus->draw(overlay);
		showScrapProcess(listMenu);
    } else if (listMenu.getNumGames() == 0){
		Fonts::drawTextCentTransparent(overlay, fontBig, emu.name.c_str(), 0, face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
			true, false, textColor, 0);
		showScrapProcess(listMenu);
		fastline(this->overlay, listMenu.marginX, listMenu.marginY - 1 , overlay->w - listMenu.marginX, listMenu.marginY - 1, menuBars);

		if (getEmuStatus() == EMU_MENU_FILTER){
			fastline(this->overlay, sepVertX, listMenu.marginY , sepVertX, listMenu.getH() + listMenu.marginY - 1, menuBars);
			drawFilters(listMenu);
		} else {
			Fonts::drawTextCent(overlay, fontsmall, "No roms found", 0, 0, true, true, textColor, 0);
		}

    } else {
		Fonts::drawTextCent(overlay, fontsmall, "The configuration is not valid", 0, 0, true, true, textColor, 0);
		Fonts::drawTextCent(overlay, fontsmall, "Press TAB to select the next entry or", 0, face_h_small + 3, true, true, textColor, 0);
		Fonts::drawTextCent(overlay, fontsmall, "Press ESC to exit", 0, (face_h_small + 3) * 2, true, true, textColor, 0);
    }
}

void GameMenu::drawSelectedGameAssets(ListMenu &listMenu, GameFile *game){
	const ConfigEmu emu = *cfgLoader->getCfgEmu();

	if (listMenu.updateAssets){
		const std::string assetsDir = dirutil::getPathPrefix(emu.assets) + std::string(Constant::tempFileSep);
		// Labels inline (bajo lock para no chocar con createMenuImages)
		if (game->gameData != NULL){
			menuTextAreas[YEAR].loadString(LanguageManager::instance()->get("menu.filter.year") + ": " + Constant::TipoToStr(game->gameData->year));
			menuTextAreas[MANUFACTURER].loadString(LanguageManager::instance()->get("menu.filter.manufacturer") + ": " + game->gameData->manufacturer);
			menuTextAreas[SYSTEM].loadString(LanguageManager::instance()->get("menu.filter.system") + ": " + listMenu.extractSystem(game->gameData->sourcefile));
		} else {
			menuTextAreas[YEAR].clear();
			menuTextAreas[MANUFACTURER].clear();
			menuTextAreas[SYSTEM].clear();
		}

		const int synMaxW = menuTextAreas[SYNOPSIS].getW() - menuTextAreas[SYNOPSIS].marginX;
		m_menuAssetLoader.submit(dirutil::getFileNameNoExt(game->longFileName),
						            assetsDir, this->overlay->format, this->overlay->w,
						            synMaxW);
	}
					
	/* Textos estaticos (YEAR/MANUFACTURER/SYSTEM): solo lectura, sin lock */
	menuTextAreas[YEAR].draw(this->overlay);
	menuTextAreas[MANUFACTURER].draw(this->overlay);
	menuTextAreas[SYSTEM].draw(this->overlay);

	if (menuTextAreas[YEAR].isEmpty() && menuTextAreas[MANUFACTURER].isEmpty() && menuTextAreas[SYSTEM].isEmpty()){
		menuTextAreas[SYNOPSIS].setY(this->overlay->h / 2 + face_h_big);
		menuTextAreas[SYNOPSIS].setH(overlay->h - menuTextAreas[SYNOPSIS].getY());
	} else if (!menuTextAreas[YEAR].isEmpty() && !menuTextAreas[MANUFACTURER].isEmpty() && !menuTextAreas[SYSTEM].isEmpty()){
		menuTextAreas[SYNOPSIS].setY(menuTextAreas[SYSTEM].getY() + face_h_big * 2 + 2);
		menuTextAreas[SYNOPSIS].setH(overlay->h - menuTextAreas[SYNOPSIS].getY());
	}

	if (listMenu.showBottomInfo){
		menuTextAreas[SYNOPSIS].setH(menuTextAreas[SYNOPSIS].getH() - face_h_big * 2);
	}

	/* SYNOPSIS: try-lock por si el worker lo esta adoptando */
	if (TryEnterCriticalSection(menuTextAreas[SYNOPSIS].m_objCS)){
		menuTextAreas[SYNOPSIS].draw(this->overlay, this->gameTicks);
		LeaveCriticalSection(menuTextAreas[SYNOPSIS].m_objCS);
	}

	/* Imagenes: try-lock independiente por asset */
	if (TryEnterCriticalSection(menuImages[SNAP].m_objCS)){
		menuImages[SNAP].printImage(this->overlay);
		LeaveCriticalSection(menuImages[SNAP].m_objCS);
	}
	if (overlay->w >= 640){
		if (TryEnterCriticalSection(menuImages[SNAPTIT].m_objCS)){
			menuImages[SNAPTIT].printImage(this->overlay);
			LeaveCriticalSection(menuImages[SNAPTIT].m_objCS);
		}
		if (TryEnterCriticalSection(menuImages[BOX2D].m_objCS)){
			menuImages[BOX2D].printImage(this->overlay);
			LeaveCriticalSection(menuImages[BOX2D].m_objCS);
		}
	}
}

void GameMenu::drawStatusBar(ListMenu &listMenu){
	static uint32_t lastTick = 0;
	static SDL_Surface *srfHora = NULL;
	static char stringLastHora[9] = {0};
	char stringHora[9] = {0}; 
	TTF_Font *fontsmall;
	
	const int x = listMenu.marginX;
	const int y = (listMenu.marginY - face_h_small) / 2;
	SDL_Rect dstRectHora;
	int mouseSupport = 0;
	SDL_Rect dstRectIcoMouse = {x, y - Icons::getInstance().icon_w_add, face_h_big, face_h_big};

	// Forzar actualizacion si es la primera vez que se ejecuta o si pasaron 10 segundos
	if (srfHora == NULL || this->gameTicks.ticks > lastTick + FPS_DESIRED * 10 || this->gameTicks.ticks == 0){
		lastTick = this->gameTicks.ticks;
		fontsmall = Fonts::getFont(Fonts::FONTSMALL);
		
		SYSTEMTIME horaLocal;
		GetLocalTime(&horaLocal);

		// Formateo seguro
		sprintf_s(stringHora, sizeof(stringHora), "%02d:%02d", horaLocal.wHour, horaLocal.wMinute);
		
		// strncmp evita leer memoria basura fuera del array de 9 bytes
		if (strncmp(stringLastHora, stringHora, 5) != 0){
			strncpy_s(stringLastHora, sizeof(stringLastHora), stringHora, _TRUNCATE);
			if (srfHora != NULL) SDL_FreeSurface(srfHora);
			srfHora = Fonts::renderUtf8Solid(fontsmall, stringHora, Constant::colors[clWhite].sdlColor);
		}
	}
	
	if (overlay == NULL) return;

	//Dibujamos si esta cargado el plugin de dashlaunch para el mouse. En Windows no lo dibujamos 
	#ifdef _XBOX
	mouseSupport = XBOX_isHidMousePluginConnected();
	if (mouseSupport){
		Icons::getInstance().drawIcon(overlay, &dstRectIcoMouse, ico_mouse);
	} else {
		dstRectIcoMouse.w = 0;
		dstRectIcoMouse.h = 0;
	}
	#else
		dstRectIcoMouse.w = 0;
		dstRectIcoMouse.h = 0;
	#endif

	//Dibujamos el tiempo
	if (srfHora != NULL){
		SDL_Rect dstRectIco = {x + dstRectIcoMouse.w / 2, y - Icons::getInstance().icon_w_add, 0, 0};
		Icons::getInstance().drawIcon(overlay, &dstRectIco, ico_clock);
		dstRectHora.x = dstRectIco.x + face_h_big + Icons::getInstance().icon_w_add;
		dstRectHora.y = y;
		dstRectHora.w = srfHora->w;
		dstRectHora.h = srfHora->h;
		SDL_BlitSurface(srfHora, NULL, overlay, &dstRectHora);
	}
}

void GameMenu::drawTitle(ListMenu &listMenu, TTF_Font *fontBig){
	const SDL_Color &textColor = Constant::colors[clWhite].sdlColor;
	const ConfigEmu emu = *cfgLoader->getCfgEmu();

	std::string title;
	if (!title_image.hasImage()){
		title = emu.name;
	} 

	if (listMenu.gameDataFields.systems.size() > 0 && (int)listMenu.gameDataFields.systems.size() > listMenu.gameDataFields.posSystem){
		if (listMenu.gameDataFields.posSystem > -1){
			title += " (" + listMenu.gameDataFields.systems[listMenu.gameDataFields.posSystem] + ")";
		} else {
			title += " " + LanguageManager::instance()->get("menu.filter.all");
		}
	}

	//Drawing the title
	if (title_image.hasImage()){
		title_image.printImage(this->overlay);
		if (!title.empty()){
			Fonts::drawTextTransparent(overlay, fontBig, title.c_str(), title_image.newOffset.w + title_image.newDim.w + 10, 
				face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
				textColor, 0);
		}
	} else {
		Fonts::drawTextCentTransparent(overlay, fontBig, title.c_str(), 0, face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
		true, false, textColor, 0);
	}
}

void GameMenu::drawInfoButtons(SDL_Rect &rect){

	if (joystick->infoButtonsDirty){
		if (infoBtnSrf){
			SDL_FreeSurface(infoBtnSrf);
			infoBtnSrf = NULL;
		}
		joystick->infoButtonsDirty = false;
	}

	if (infoBtnSrf != NULL){
		SDL_BlitSurface(infoBtnSrf, NULL, overlay, &rect);
		return;
	}

	TTF_Font *fontBig = Fonts::getFont(Fonts::FONTBIG);
	TTF_Font *fontSmall = Fonts::getFont(Fonts::FONTSMALL);

	const int posY = rect.h / 2;
	int x = 10;
	
	SDL_Surface *txtSrf = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, rect.h, overlay->format->BitsPerPixel, 
			overlay->format->Rmask, overlay->format->Gmask, overlay->format->Bmask, overlay->format->Amask);
	
	const int buttonsColor = clWhite;
	const int buttonsTransparentColor = clBlack;

	//Setting the transparency effect
	Uint32 colorkey = SDL_MapRGBA(txtSrf->format,
                                            Constant::colors[buttonsTransparentColor].sdlColor.r,
                                            Constant::colors[buttonsTransparentColor].sdlColor.g,
                                            Constant::colors[buttonsTransparentColor].sdlColor.b,
                                            0xFF);

    //SDL_SetColorKey(txtSrf, SDL_SRCCOLORKEY, colorkey);
	//SDL_SetAlpha(txtSrf, SDL_RLEACCEL, colorkey);
	SDL_FillRect(txtSrf, NULL, colorkey);

	SDL_Rect drawnRect = {0, 0, 0, 0};
	int tw = 0, th = 0;
	const int circleCenterY = posY;
	
	for (unsigned int i=0; i < joystick->infoButtons.size(); i++){
		Fonts::getSize(fontSmall, joystick->infoButtons[i].text, tw, th);
		const int circleCenterX = x + tw / 2;

		if (joystick->infoButtons[i].shape == BS_CIRCLE){
			filledCircleColor(txtSrf, circleCenterX, circleCenterY, face_h_big / 2, Constant::colors[buttonsColor].colorRaw);
		} else if (joystick->infoButtons[i].shape == BS_DOUBLE_CIRCLE){
			filledCircleColor(txtSrf, circleCenterX, circleCenterY, (int)(face_h_big * 4 / 5.0f), Constant::colors[buttonsColor].colorRaw);
			filledCircleColor(txtSrf, circleCenterX, circleCenterY, (int)(face_h_big * 2 / 3.0f + 2), Constant::colors[buttonsTransparentColor].colorRaw);
			filledCircleColor(txtSrf, circleCenterX, circleCenterY, face_h_big / 2 + 2, Constant::colors[buttonsColor].colorRaw);
		}
		
		drawnRect = Fonts::drawText(txtSrf, fontSmall, joystick->infoButtons[i].text.c_str(), x, posY - face_h_small / 2, Constant::colors[buttonsTransparentColor].sdlColor, 0);

		if (!joystick->infoButtons[i].mergeNext){
			x += drawnRect.w + face_h_big / 2;
		} else {
			x += drawnRect.w;
		}
		
		if (!joystick->infoButtons[i].mergeNext){
			std::string desc = joystick->infoButtons[i].description;
			if (i > 0 && joystick->infoButtons[i-1].mergeNext){
				desc = reduceWords(joystick->infoButtons[i-1].description, joystick->infoButtons[i].description);
			}
			drawnRect = Fonts::drawText(txtSrf, fontBig, desc.c_str(), x, posY - face_h_big / 2, Constant::colors[buttonsColor].sdlColor, 0);
		} else {
			drawnRect.w = 0;
		}

		if (joystick->infoButtons[i].mergeNext){
			x += drawnRect.w + (i + 1 < joystick->infoButtons.size() ? face_h_big : 0);
		} else {
			x += drawnRect.w + (i + 1 < joystick->infoButtons.size() ? face_h_big : 0);
		}
	}

	drawnRect.w = x;
	drawnRect.x = 0;
	drawnRect.y = 0;
	drawnRect.h = rect.h;

	SDL_Rect dstRect = { (rect.w - drawnRect.w) / 2, rect.y, 0, 0};

	infoBtnSrf = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, rect.h, overlay->format->BitsPerPixel, 
			overlay->format->Rmask, overlay->format->Gmask, overlay->format->Bmask, overlay->format->Amask);
	SDL_FillRect(infoBtnSrf, NULL, colorkey);
	
	SDL_Rect dstRect2 = { (rect.w - drawnRect.w) / 2, 0, 0, 0};
	SDL_BlitSurface(txtSrf, &drawnRect, infoBtnSrf, &dstRect2);
	SDL_BlitSurface(infoBtnSrf, NULL, overlay, &rect);
	SDL_SetAlpha(infoBtnSrf, SDL_RLEACCEL, 0xFF);

	//SDL_BlitSurface(txtSrf, &drawnRect, overlay, &dstRect);
	SDL_FreeSurface(txtSrf);
}

std::string GameMenu::reduceWords(const std::string &sentence1, const std::string &sentence2) {
    // 1. Unir todas las palabras en un solo flujo manteniendo el orden original
    std::vector<std::string> allWords = Constant::splitChar(sentence1, ' ');
    std::vector<std::string> v2 = Constant::splitChar(sentence2, ' ');
    allWords.insert(allWords.end(), v2.begin(), v2.end());

    // 2. Contar frecuencias y registrar la primera aparicion de cada palabra
    std::map<std::string, int> counts;
    std::map<std::string, std::size_t> firstAppearance;
    
    for (std::size_t i = 0; i < allWords.size(); ++i) {
        const std::string &w = allWords[i];
        counts[w]++;
        if (firstAppearance.find(w) == firstAppearance.end()) {
            firstAppearance[w] = i;
        }
    }

    // 3. Pasar los datos unicos a un vector para poder ordenarlos
    std::vector<WordFreq> sortedWords;
    std::map<std::string, int>::iterator it;
    for (it = counts.begin(); it != counts.end(); ++it) {
        WordFreq wf = { it->first, it->second, firstAppearance[it->first] };
        sortedWords.push_back(wf);
    }

    // 4. Ordenar: mayor frecuencia primero
    std::sort(sortedWords.begin(), sortedWords.end());

    // 5. Construir la frase final respetando la logica de separadores
    std::string finalSentence;
    int lastWordHits = 0;

    for (std::size_t i = 0; i < sortedWords.size(); ++i) {
        const std::string &elem = sortedWords[i].word;
        
        // '/' si la anterior no se repetia, ' ' si se repetia
        std::string sep = (lastWordHits == 0) ? "/" : " ";
        finalSentence += finalSentence.empty() ? elem : sep + elem;

        // Si la palabra actual se repite (count > 1), marcamos hit
        lastWordHits = (sortedWords[i].count > 1) ? 1 : 0;
    }

	// CORRECCION: Forzar la primera letra a mayúscula si el texto no esta vacio
    if (!finalSentence.empty()) {
        finalSentence[0] = static_cast<char>(::toupper(static_cast<unsigned char>(finalSentence[0])));
    }

    return finalSentence;
}


void GameMenu::initAchievements(){
	const bool loadAchievement = getCfgLoader()->configMain[cfg::enableAchievements].valueBool;
	if (loadAchievement){
		Achievements::instance()->setHardcoreMode(getCfgLoader()->configMain[cfg::hardcoreRA].valueBool);
		const std::string user = getCfgLoader()->configMain[cfg::raUser].valueStr;
		const std::string pass = getCfgLoader()->configMain[cfg::raPass].valueStr;
		Achievements::instance()->login(user.c_str(), pass.c_str());
	}
}

/**
* se traduce el id del sistema, que debe corresponder a la lista de consolas definidas en #include <rc_consoles.h>
*/
int GameMenu::translateSystemAchievement(){
	vector<string> v = Constant::splitChar(getCfgLoader()->getCfgEmu()->system, '_');
	int system = 0;
	if (v.size() > 0){
		system = Constant::strToTipo<int>(v.at(0));
		return gsTogdGameid[system];
	}
	return 0;
}

/**
* se obtienen los id's de las traducciones del sistema de logros, que debe corresponder a la 
* lista de consolas definidas en #include <rc_consoles.h>
*/
bool GameMenu::cargarSystemAchievementTranslation(const std::string& nombreArchivo) {
    std::ifstream file(nombreArchivo.c_str());
    std::string line;
    bool seccionEncontrada = false;

    if (!file.is_open()) return false;

    while (std::getline(file, line)) {
        // Eliminar espacios en blanco al inicio/final si fuera necesario (opcional)
            
        // 1. Buscamos el inicio de la seccion
        if (line == "[SCREENSCRAPPER_TO_ACHIEVEMENTS]") {
            seccionEncontrada = true;
            continue; // Pasamos a la siguiente linea
        }

        // 2. Si ya estamos en la seccion correcta, procesamos los datos
        if (seccionEncontrada) {
            // Si encontramos otra seccion (empieza por [), dejamos de leer
            if (!line.empty() && line[0] == '[') {
                break; 
            }

            // Ignorar comentarios o lineas vacias
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // 3. Extraer clave=valor
            std::size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string keyStr = line.substr(0, pos);
                std::string valueStr = line.substr(pos + 1);

				int key = Constant::strToTipo<int>(keyStr);
                int value = Constant::strToTipo<int>(valueStr);

                gsTogdGameid[key] = value;
            }
        }
    }
    file.close();
    return seccionEncontrada;
}

/**
*
*/
void GameMenu::loadGameAchievements(unzippedFileInfo& unzipped){
	LOG_DEBUG("Unload achievements");
    // 1. Limpiar SIEMPRE antes de configurar nada nuevo
    Achievements::instance()->doUnload();

	LOG_DEBUG("Checking hardcore mode");
    // 2. Configurar el modo (Hardcore/Softcore)
    Achievements::instance()->setHardcoreMode(getCfgLoader()->configMain[cfg::hardcoreRA].valueBool);

	LOG_DEBUG("Getting libretro memory");
    // 3. Obtener punteros de memoria (Asegarate de que el Core ya esta cargado)
    uint8_t* w_data = (uint8_t*)retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
    std::size_t w_size = retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
    uint8_t* s_data = (uint8_t*)retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    std::size_t s_size = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);

	LOG_DEBUG("Setting memory sources");
    // 4. Pasar a la clase de logros
    Achievements::instance()->set_memory_sources(w_data, w_size, s_data, s_size);

	LOG_DEBUG("getting memory descriptors");
	// 4b. Pasar los descriptores de memoria del core (si los envio via SET_MEMORY_MAPS)
	{
		unsigned desc_count = 0;
		const struct retro_memory_descriptor* descs = get_core_memory_descriptors(&desc_count);
		/* Siempre actualizar si desc_count==0 sobreescribimos cualquier
		 * descriptor heredado de la carga anterior (defensa en profundidad
		 * frente a cores que no reemiten SET_MEMORY_MAPS entre juegos). */
		Achievements::instance()->set_core_descriptors(desc_count > 0 ? descs : NULL, desc_count);
	}

	LOG_DEBUG("Translating achievements");
    // 5. Cargar logica del juego
    int system = translateSystemAchievement();
    std::string pathToRom = unzipped.extractedPath.empty() ? unzipped.originalPath : unzipped.extractedPath;

	Achievements::instance()->setScreenFormat(this->overlay->format);
	LOG_DEBUG("Loading achievements from game");
    Achievements::instance()->load_game((uint8_t *)unzipped.memoryBuffer, unzipped.romsize, pathToRom, system, messagesAchievement);
	
}

std::string GameMenu::configButtonsJOY(){
    bool salir = false;
    string salida = "";
	const int TIMETOLIMITFRAME = (int)(1000 / 30.0);
	int mNumJoysticks = SDL_NumJoysticks();
	std::map<int, int>* mPrevAxisValues; //Almacena los valores de los ejes de cada joystick
	std::map<int, int>* mPrevHatValues; //Almacena los valores de las crucetas de cada joystick
	mPrevAxisValues = new std::map<int, int>[mNumJoysticks];
    mPrevHatValues = new std::map<int, int>[mNumJoysticks];

	int buttons = SDL_JoystickNumButtons(joystick->g_joysticks[0]);
    long delay = 0;
    unsigned long before = 0;
    //Posiciones de los botones calculadas en porcentaje respecto al alto y ancho de la imagen
    //t_posicion_precise imgButtonsRelScreen[] = {{0.3512,0.682,0,0},{0.3512,0.84,0,0},{0.295,0.76,0,0},{0.4075,0.76,0,0},
    //        {0.79375,0.616,0,0},{0.87625,0.496,0,0},{0.2225,0.194,0,0},{0.7775,0.194,0,0},{0.39375,0.512,0,0},{0.60875,0.512,0,0}};

    int tam = 14;
    int i=0;
    /*UIPicture obj;

    obj.setX(0);
    obj.setY(0);
    obj.setW(this->getWidth());
    obj.setH(this->getHeight());
    obj.loadImgFromFile(Constant::getAppDir() +  Constant::getFileSep() + "imgs" + Constant::getFileSep() + "xbox_360_controller-small.png");
    //Para que se guarde la relacion de aspecto
    obj.getImgGestor()->setBestfit(false);
    //Para redimensionar la imagen al contenido
    obj.getImgGestor()->setResize(true);*/

    do{
        //SDL_Event event;
        before = SDL_GetTicks();

        /*if (!obj.getImgDrawed()){
            //Limpiamos la pantalla
            clearScr(cBgScreen);
            //Dibujamos la imagen
            drawImgObj(&obj);
            //Obtenemos las variables que indican la posicion de la imagen una vez que
            //ha sido pintada por pantalla
            int imgX = obj.getImgGestor()->getImgLocationRelScreen().x;
            int imgY = obj.getImgGestor()->getImgLocationRelScreen().y;
            int imgW = obj.getImgGestor()->getImgLocationRelScreen().w;
            int imgh = obj.getImgGestor()->getImgLocationRelScreen().h;
            double relacionAncho =  obj.getImgGestor()->getImgOrigWidth() > 1 ? this->getWidth() / (double) obj.getImgGestor()->getImgOrigWidth()  : 0.2;
            double relacionAlto =  obj.getImgGestor()->getImgOrigHeight() > 1 ? this->getHeight() / (double) obj.getImgGestor()->getImgOrigHeight()  : 0.2;
            //Marcamos la posicion del boton que hay que pulsar
            pintarCirculo(imgW * imgButtonsRelScreen[i].x + imgX, imgh * imgButtonsRelScreen[i].y + imgY, 40 * (relacionAncho < relacionAlto ? relacionAncho : relacionAlto), cRojo);
//            pintarFillCircle(overlay,
//                             imgW * imgButtonsRelScreen[i].x + imgX,
//                             imgh * imgButtonsRelScreen[i].y + imgY,
//                             40 * relacionAncho,
//                             SDL_MapRGB(overlay->format, 255,0,0));

            //Dibujamos el texto de la accion
            drawTextCent(JoystickButtonsMSG[i], 0, 20, true, false, cBlanco);
            cachearObjeto(&obj);
        } else {
            cachearObjeto(&obj);
        }*/
		
		SDL_FillRect(this->overlay, NULL, Constant::colors[clBackground].color);
		Fonts::drawTextCent(this->overlay, Fonts::getFont(Fonts::FONTSMALL), FRONTEND_BTN_TXT[i].c_str(), 0, 20, true, false, Constant::colors[clWhite].sdlColor, 0);
        salviaFlip(this->gameScreen);
		/*
        while( SDL_PollEvent( &event ) ){
             switch( event.type ){
                case SDL_QUIT:
                    salir = true;
                    break;
                case SDL_KEYDOWN: // PC buttons
                    if (event.key.keysym.sym == SDLK_ESCAPE){
                        salir = true;
                    }
                    break;
                case SDL_JOYBUTTONDOWN :
					joystick->buttonsMapperFrontend.buttons[event.jbutton.button] = JoyButtonsVal[i];
                    i++;
                    break;
                case SDL_JOYHATMOTION:
					if (event.jhat.value != 0){ //Solo en el momento del joydown
						if (event.jhat.value & SDL_HAT_UP){
							joystick->buttonsMapperFrontend.hats[event.jhat.value & SDL_HAT_UP] = JoyButtonsVal[i];
						} else if (event.jhat.value & SDL_HAT_DOWN){
							joystick->buttonsMapperFrontend.hats[event.jhat.value & SDL_HAT_DOWN] = JoyButtonsVal[i];
						} else if (event.jhat.value & SDL_HAT_LEFT){
							joystick->buttonsMapperFrontend.hats[event.jhat.value & SDL_HAT_LEFT] = JoyButtonsVal[i];
						} else if (event.jhat.value & SDL_HAT_RIGHT){
							joystick->buttonsMapperFrontend.hats[event.jhat.value & SDL_HAT_RIGHT] = JoyButtonsVal[i];
						}
                        i++;
                    }
                    break;
                case SDL_JOYAXISMOTION:
                    //int normValue;
                    if((abs(event.jaxis.value) > DEADZONE) != (abs(mPrevAxisValues[event.jaxis.which][event.jaxis.axis]) > DEADZONE)){
						if (abs(event.jaxis.value) > DEADZONE) {
							// 0 si es negativo (Izquierda/Arriba), 1 si es positivo (Derecha/Abajo)
							int isPositive = (event.jaxis.value > 0);
							int buttonIdx = (event.jaxis.axis * 2) + isPositive;

							LOG_DEBUG("Eje: %d, Valor: %d -> Boton Virtual: %d", 
									   event.jaxis.axis, event.jaxis.value, buttonIdx);

							joystick->buttonsMapperFrontend.axis[buttonIdx] = JoyButtonsVal[i++];
						} else {
							// CENTRO: Opcionalmente manejar el reposo aqui si es necesario
						}
                    }
                    mPrevAxisValues[event.jaxis.which][event.jaxis.axis] = event.jaxis.value;
                    break;
             }
        }*/

		//buttons + 4 -> sumando las 4 crucetas
        if (i == tam || i >= buttons + 4){
            salir = true;
        }

        delay = before - SDL_GetTicks() + TIMETOLIMITFRAME;
        if(delay > 0) SDL_Delay(delay);
    } while (!salir);

    //joystick->saveButtonsFrontend(Constant::getAppDir() + Constant::getFileSep() + "joystick.ini");
    return salida;
}

bool GameMenu::isDebug(){
	bool debug;
	getCfgLoader()->configMain[cfg::debug].getPropValue(debug);
    return debug;
}

void GameMenu::setCfgLoader(CfgLoader *cfgLoader){
    this->cfgLoader = cfgLoader;
}

CfgLoader * GameMenu::getCfgLoader(){
	return this->cfgLoader;
}

void GameMenu::drawFilters(ListMenu &listMenu){
	int x = menuTextAreas[SYNOPSIS].getX() + listMenu.marginX;
	int y = menuImages[SNAP].getY();
	int w = menuTextAreas[SYNOPSIS].getW() - 2*listMenu.marginX;
	std::string val;
	TTF_Font *fontbig = Fonts::getFont(Fonts::FONTBIG);
	SDL_Color lineTextColor;
	const int sw_h = face_h_big - 2;
	const int sw_w = sw_h * 2;
	auto opciones = configMenus->menuGameFilter->opciones;
	const SDL_Color &black = Constant::colors[clBlack].sdlColor;
	const SDL_Color &white = Constant::colors[clWhite].sdlColor;
	const SDL_Color &menuBars = Constant::colors[clMenuBars].sdlColor;
	SDL_Rect rectElem = {x, y, w, face_h_big};
	
	if (filterAlphaRec == NULL || filterAlphaRec->w != rectElem.w || filterAlphaRec->h != rectElem.h){
		if (filterAlphaRec != NULL){
			SDL_FreeSurface(filterAlphaRec);
		}
		Constant::createRectAlphaFilled(filterAlphaRec, rectElem, overlay->format, clBkgMenu, true);
	}

	JFY_TYPE justifyCenter(JFY_CENTER, w);

	std::string filterTitle = Constant::string_format(LanguageManager::instance()->get("menu.filter.number"), listMenu.filteredGames.size());
	SDL_Rect fillHeaderRect = {x, y, w, face_h_big + 3};
	fastline(this->overlay, x, y + face_h_big + 3, x + w, y + face_h_big + 3, menuBars);
	Fonts::drawTextTransparent(overlay, fontbig, filterTitle.c_str(), x, y, Constant::colors[clWhite].sdlColor, 0, justifyCenter);
	y += face_h_big + 7;

	for (int i=0; i < (int)opciones.size(); i++){
		if (i == configMenus->menuGameFilter->seleccionado){
			rectElem.y = y + i * (face_h_big + 3);
			if (filterAlphaRec)
				SDL_BlitSurface(filterAlphaRec, NULL, overlay, &rectElem);
			lineTextColor = black;
		} else {
			lineTextColor = white;
		}

		SDL_Rect dimLeft = Fonts::drawTextTransparent(overlay, fontbig, opciones[i]->titulo.c_str(), x, y + i * (face_h_big + 3), lineTextColor, 0);
		const int leftTxtDim = dimLeft.w + 3;
		JFY_TYPE justifyHelper(JFY_RIGHT, w - leftTxtDim);

		if (opciones[i]->tipo == OPC_LISTA_REF){
			OpcionListaRef* l = (OpcionListaRef*)opciones[i];
			int num = (int)(*l->items).size();
			int idx = *l->indice;

			if (idx <= -1){
				val = LanguageManager::instance()->get("menu.filter.all");
			} else {
				val = (*l->items).at(idx);
				//Lamentablemente la informacion de los anyos de mame no esta bien especificada.
				//revisar mameparser.h -> convertYear si se quiere saber como se clasifican los anyos con el simbolo ?
				if (val.length() == 2){
					if (val == "20"){
						val = "2000's";
					} else {
						val = "19" + val + "'s";
					}
				} else if (val == "9999"){
					val = LanguageManager::instance()->get("menu.filter.unknown");
				}
			}
			SDL_Rect dimRight = Fonts::drawTextTransparent(overlay, fontbig, val.c_str(), x + leftTxtDim, y + i * (face_h_big + 3), lineTextColor, 0, justifyHelper);

		} else if (opciones[i]->tipo == OPC_BOOLEANA){
			OpcionBool* b = (OpcionBool*)opciones[i];
			bool enabled = *b->valor;
			// 2. Dibujar el fondo del switch
			const int sw_x = x + leftTxtDim + justifyHelper.getJustification(sw_w);
			const int sw_y = y + i * (face_h_big + 3) + 1;
			SDL_Rect baseRect = {sw_x, sw_y, sw_w, sw_h };
			SDL_FillRect(overlay, &baseRect, enabled ? Constant::colors[clSwitchEnabled].color : Constant::colors[clSwitchDisabled].color);

			// 3. Calcular el thumb (boton interno) de forma relativa
			const int spacing = 4;
			const int size = sw_h - (spacing * 2);
			int thumbX = sw_x + (enabled ? (sw_w - size - spacing) : spacing);

			SDL_Rect thumbRect = { thumbX, sw_y + spacing, size, size };

			// 4. Dibujar el thumb segun el estado
			if (enabled) {
				SDL_FillRect(overlay, &thumbRect, Constant::colors[clBlack].color);
			} else {
				// Usando los campos de thumbRect directamente para evitar sumas manuales
				rect(overlay, thumbRect.x, thumbRect.y, thumbRect.x + size, thumbRect.y + size, black);
				rect(overlay, thumbRect.x + 1, thumbRect.y + 1, thumbRect.x + size - 1, thumbRect.y + size - 1, black);
			}
		}
	}
}

/**
*
*/
void GameMenu::showScrapProcess(ListMenu &listMenu){
	TTF_Font *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
    const int halfWidth = overlay->w / 2;
	const SDL_Color &textColor = Constant::colors[clWhite].sdlColor;

	if (Scrapper::isScrapping()){
		Scrapper::g_status.procesados;
		std::string str = "Scrapping: " + Constant::TipoToStr(Scrapper::g_status.procesados) + "/" + Constant::TipoToStr(Scrapper::g_status.total); 
		if (Scrapper::g_status.remainingMedia > 0){
			str += " - " + LanguageManager::instance()->get("msg.download.media") + " " + Constant::TipoToStr(Scrapper::g_status.remainingMedia);
		}
		std::string str2 = std::string(Scrapper::g_status.emuActual) + " - " + std::string(Scrapper::g_status.juegoActual);
		Fonts::drawTextTransparent(overlay, fontsmall, str.c_str(), halfWidth + halfWidth / 4, face_h_small < listMenu.marginY ? (listMenu.marginY - face_h_small) / 2 - face_h_small / 2 : 0 , textColor, 0);
		Fonts::drawTextTransparent(overlay, fontsmall, str2.c_str(), halfWidth + halfWidth / 4, face_h_small < listMenu.marginY ? (listMenu.marginY - face_h_small) / 2 + face_h_small / 2: 0 , textColor, 0);
	}

	if (Scrapper::g_status.abortType == ABORT_LIMIT_CUOTA){
		showSystemMessage(LanguageManager::instance()->get("msg.scrap.abort.cuota"), 3000);
		Scrapper::g_status.abortType = ABORT_NONE;
	} else if (Scrapper::g_status.abortType == ABORT_SCRAP_END){
		configMenus->stopScrapping(NULL);
		Scrapper::g_status.abortType = ABORT_NONE;
	}
}

/**
 * 
 */
void GameMenu::showMessage(string msg){
	if (msg.empty()) return;

    int startGray = 240;
    static const int bkg = SDL_MapRGB(this->overlay->format, startGray, startGray, startGray);
    TTF_Font *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
    
    int rw = Fonts::getSize(Fonts::FONTSMALL, msg) + 5; 
    int rh = face_h_small * 2;
    int rx = (this->overlay->w - rw) / 2;
    int ry = (this->overlay->h - rh) / 2 + face_h_small / 2;

	SDL_Rect rect = {rx, ry, rw, rh};
    SDL_FillRect(this->overlay, &rect, bkg);
    
    const int step = 40;
    for (int i=1; i < 5; i++){
        int fadingBkg = SDL_MapRGB(this->overlay->format, startGray - i*step, startGray - i*step, startGray - i*step);
		//drawing_mode(DRAW_MODE_TRANS, this->overlay, rx, ry);
		rectangleColor(this->overlay, rx - i, ry - i, rx + rw + i, ry + rh + i, fadingBkg);
    }

    //drawing_mode(DRAW_MODE_SOLID, this->overlay, rx, ry);
	Fonts::drawTextCent(overlay, fontsmall, msg.c_str(), 
		this->overlay->w / 2, this->overlay->h / 2, true, true, Constant::colors[clBlack].sdlColor, -1);
}

/**
 * 
 */
void GameMenu::loadEmuCfg(ListMenu &menuData){
    TTF_Font *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
	const int& cblack = Constant::colors[clBlack].color;
	const SDL_Color& white = Constant::colors[clWhite].sdlColor;

    if (cfgLoader->emulators.size() == 0){
        SDL_FillRect(overlay, NULL, cblack);
        string msg = "There are no emulators configured. Exiting..."; 
		Fonts::drawTextCent(overlay, fontsmall, msg.c_str(), 0, 0, true, true,  white, -1);
		Fonts::drawTextCent(overlay, fontsmall, "Press a key to continue", 0, face_h_small + 3, true, true, white, -1);
		salviaFlip(gameScreen);
        SDL_Delay(3000);
		return;
    }

	if (cfgLoader->emulators.size() <= (std::size_t)cfgLoader->emuCfgPos){
        cfgLoader->emuCfgPos = 0;
    } 

    dirutil dir;
	ConfigEmu *emu = cfgLoader->getCfgEmu();
    string mapfilepath = Constant::getAppDir()
            + string(Constant::tempFileSep) + "config" + string(Constant::tempFileSep) + emu->map_file;
    
    if (emu->use_rom_file && !emu->map_file.empty() && dir.fileExists(mapfilepath.c_str())){
        menuData.mapFileToList(mapfilepath);
    } else if (!emu->rom_directory.empty()){
		mapfilepath = dirutil::getPathPrefix(emu->rom_directory, CfgLoader::configMain[cfg::roms_path].valueStr);
		std::string relativePath = menuData.listDir.getRelativePath();
		if (!relativePath.empty()){
			mapfilepath.append(Constant::getFileSep() + relativePath);
			menuData.resizeMarginTop(face_h_big, overlay->h);
		}

		LOG_DEBUG("Listing directory: %s", mapfilepath.c_str());
        vector<unique_ptr<FileProps>> files;
		string extFilter = " " + emu->rom_extension;
        extFilter = Constant::replaceAll(extFilter, " ", ".");

		if (emu->menu_directory_recursive){
			dir.listFilesRecursive(mapfilepath.c_str(), files, extFilter, "", false, false, false);
		} else {
			dir.listFiles(mapfilepath.c_str(), files, extFilter, "", emu->menu_show_directories, false, false);
		}
		
        menuData.filesToList(files, *emu);
        files.clear();
    } else {
		menuData.clear();
	}

	//Se inician los menus de los filtros
	configMenus->iniciarFiltros(menuData.gameDataFields);

	//Se cargan las imagenes
	const std::string assetsDir = getAssetsDir(emu);
	const std::string packetAssetsFile = dir.getPathPrefix(assetsDir + Constant::getFileSep() + SCRAPPING_DAT);

	if (dir.fileExists(packetAssetsFile.c_str())){
		filePackage.Load(packetAssetsFile.c_str());
	} 
	//else {
	//	filePackage.Pack(dir.getPathPrefix(emu->assets), packetAssetsFile.c_str());
	//	LOG_DEBUG("Creating assets in %s\n", emu->assets.c_str());
	//}
	loadBgImageAndTitleEmu();	
}

void GameMenu::loadBgImageAndTitleEmu(){
	ConfigEmu *emu = cfgLoader->getCfgEmu();
	const std::string assetsDir = getAssetsDir(emu);
	dirutil dir;

	//Se carga la imagen del emulador que va en el titulo
	std::string titleEmuImg = dir.getPathPrefix(assetsDir + Constant::getFileSep() + TITLE_EMU_FILENAME);
	if (dir.fileExists(titleEmuImg.c_str())){
		if (title_image.getFilepath() != titleEmuImg)
			title_image.loadImage(titleEmuImg, NULL);
	} else {
		title_image.closeImage();
	}

	//Loading the background image if exists
	loadBgImage();
}

string GameMenu::getAssetsDir(ConfigEmu *emu){
	return emu->title_bkg_assets.empty() ? emu->assets : emu->title_bkg_assets;
}

/**
 * 
 */
string GameMenu::encloseWithCharIfSpaces(string str, string encloseChar){
    str = Constant::Trim(str);
    return str.find(" ") != string::npos ? encloseChar + str + encloseChar : str;
}

/**
 * 
 */
vector<string> GameMenu::launchProgram(const std::string& fullPathRom){
    dirutil dir;
    vector<string> commands;
	Launcher launcher;

    if (cfgLoader->emulators.size() <= (std::size_t)cfgLoader->emuCfgPos)
        return commands;

	ConfigEmu* emu = cfgLoader->getCfgEmu();
	const std::string pathExecutable = dirutil::getPathPrefix(emu->executable, dirutil::getPathPrefix(emu->directory));
	//Setting the executable if needed to launch a different emulator than the current one
	commands.emplace_back(pathExecutable);

	#ifdef _XBOX
		//A la xbox le da igual si el path tiene espacios
		commands.emplace_back(fullPathRom);
	#else
		commands.emplace_back(encloseWithCharIfSpaces(fullPathRom, "\"")); 
	#endif

	//Comprobamos si hay que lanzar el emulador correspondiente
	if (!emuCanLaunchGame()){
		LOG_DEBUG("Launching %s %s\n", emu->executable.c_str(), fullPathRom.c_str());
		this->running = !launcher.launch(commands);
	} 
	// Si llegamos aqui, tenemos que lanzar la rom en el propio ejecutable porque el soporte es correcto para esta rom
	return commands;
}

/**
* Comprueba si el emulador para ejecutar el juego es el que hay cargado actualmente.
* Hay algunos cores que no se desinicializan correctamente e implica mucha investigacion
* portearlos. Para esos casos (ej: PRBOOM), se define el preprocesador RELOAD_CORE, para que
* haga un reinicio de todo el frontend. Esto tiene sus desventajas, pero es la forma mas limpia
* de mantenerse actualizado upstream con el git de algunos cores
*
* Devuelve true si se puede abrir el juego con el core actual
* Devuelve false si hay que cargar otro core
*/
bool GameMenu::emuCanLaunchGame(){
#ifndef RELOAD_CORE
	ConfigEmu* emu = cfgLoader->getCfgEmu();
	const std::string execActual = Constant::getAppExecutable();
	return emu->executable.find(execActual) != string::npos;
#else
	return false;
#endif
}

/**
* To list the contents of a zip file and be able to load a rom, the name of the zip file
* should begin with a character "@"
*/
FILE_STATUS GameMenu::listableZip(ListMenu &listMenu, FILE_NAVIGATION nav){
	dirutil dir;
	ConfigEmu emu = *cfgLoader->getCfgEmu();
	FILE_STATUS ret = FS_NOZIP_TO_LIST;
	string romFile;
	string romFileName;

	if (listMenu.curPos >= 0 && listMenu.curPos < (int)listMenu.filteredGames.size()){
		auto game = listMenu.filteredGames.at(listMenu.curPos);
		romFile = game->longFileName;
		romFileName = dir.getFileName(game->longFileName);
	}

	bool selectedListableZip = !romFile.empty() && !romFileName.empty() && romFileName[0] == '@' && nav == FS_ZIP_CD;

	if ( selectedListableZip || !listMenu.listZipped.getInternalDir().empty() || !listMenu.listZipped.file.empty()){
		//Try to list the contents of the directory
		if (selectedListableZip){
			if (!emu.menu_directory_recursive){
				listMenu.listZipped.dir = emu.use_rom_directory ? dirutil::getPathPrefix(emu.rom_directory, CfgLoader::configMain[cfg::roms_path].valueStr) + string(Constant::tempFileSep) : "";
				listMenu.listZipped.file = emu.use_extension ? romFile : dir.getFileNameNoExt(romFile);
			} else {
				listMenu.listZipped.dir = emu.use_rom_directory ? dir.getFolder(romFile) + Constant::getFileSep() : "";
				listMenu.listZipped.file = emu.use_extension ? dir.getFileName(romFile) : dir.getFileNameNoExt(romFile);
			}
		} 
		std::string romzip = listMenu.listZipped.dir + listMenu.listZipped.file;

		// Abrir el ZIP
		ZipBrowser zb;
		if (!zb.Open(romzip)){
			LOG_ERROR("Error: no se pudo abrir el fichero: %s", romzip.c_str());
			return ret;
		}
		LOG_DEBUG("Fichero abierto correctamente: %s", romzip.c_str());
		
		std::string internalPath = listMenu.listZipped.getInternalDir();
		if (!selectedListableZip && nav == FS_ZIP_CD){
			internalPath += romFile;
		} 

		ZipBrowser::PathType pathType = zb.GetPathType(internalPath);

		if (pathType == ZipBrowser::PATH_DIR){
			//internalPath += Constant::getFileSep();
			listMenu.listZipped.entries = zb.ListDirectory(internalPath);
			if (!listMenu.listZipped.entries.empty()){
				listMenu.clear();
				listMenu.zippedToList(Constant::strToTipo<int>(emu.system));
				//Solo actualizamos el path interno si es un directorio
				if (nav == FS_ZIP_CD && (selectedListableZip || !romFile.empty())){
					listMenu.listZipped.cd(romFileName[0] != '@' ? romFileName : "");
				}
			}
			ret = FS_ZIP_NAVIGATION;
		} else if (pathType == ZipBrowser::PATH_FILE){
			// Extracting the file. We try to extract the file as is, with it's original name, but it can cause filesystem
			// issues due to illegal characters. In order to address that, first we try the original name. If it was 
			// unsuccesfull, we try removing those illegal characters. If after all, we have another error, we try to generate
			// a MD5 hash and extract it with that name.
			// We are on the need to generate a unique name to don't affect to the sram file save, since the name is used to 
			// store the information and therefore, we will lose the data after loading a different game

			std::string extractionPath = Constant::getTmpDir() + Constant::getFileSep() + dir.getFileName(internalPath);
			//First try. Extract the current internal name file
			ret = extractFileFromZip(internalPath, extractionPath, zb, listMenu);
			
			if (ret != FS_ZIP_FILE_EXTRACTED){
				//Error Extract the file trying to remove some special characters
				LOG_ERROR("Error extracting %s Trying to delete special chars...", extractionPath.c_str());
				extractionPath = Constant::getTmpDir() + Constant::getFileSep() + Constant::checkPath(internalPath);
				ret = extractFileFromZip(internalPath, extractionPath, zb, listMenu);
			}

			if (ret != FS_ZIP_FILE_EXTRACTED){
				//Extract the file generating an md5 hash of the full filename path "\1 US - Q-Z\Sonic The Hedgehog 3 (USA).md"
				LOG_ERROR("Error extracting %s Trying to generate an md5 hash...", extractionPath.c_str());
				extractionPath = Constant::getTmpDir() + Constant::getFileSep() + GetMD5(internalPath) + dir.getExtension(internalPath);
				ret = extractFileFromZip(internalPath, extractionPath, zb, listMenu);
			}
			
			if (ret != FS_ZIP_FILE_EXTRACTED){
				//We give up
				LOG_ERROR("Unable to extract file");
			}
		} 
		zb.Close();
	}

	if (ret != FS_NOZIP_TO_LIST){
		//Cambiamos el tamanyo del listado para poder mostrar la ruta relativa de directorios
		listMenu.resizeMarginTop(face_h_big, overlay->h);
	}
	return ret;
}

FILE_STATUS GameMenu::listableDir(ListMenu &listMenu, FILE_NAVIGATION nav){
	dirutil dir;
	ConfigEmu emu = *cfgLoader->getCfgEmu();
	FILE_STATUS ret = FS_DIR_EMPTY;
	string romFile;

	if (listMenu.curPos >= 0 && listMenu.curPos < (int)listMenu.filteredGames.size()){
		auto game = listMenu.filteredGames.at(listMenu.curPos);
		romFile = game->longFileName;
	}
	
	listMenu.listDir.dir = emu.use_rom_directory ? dirutil::getPathPrefix(emu.rom_directory, CfgLoader::configMain[cfg::roms_path].valueStr) + string(Constant::tempFileSep) : "";
	
	std::string fileSelected;
	std::string rompath;

	if (nav == FS_DIR_CD){
		fileSelected = emu.use_extension ? romFile : dir.getFileNameNoExt(romFile);
		rompath = listMenu.listDir.dir;
		std::string relativePath = listMenu.listDir.getRelativePath();
		if (!relativePath.empty()){
			rompath.append(relativePath + Constant::getFileSep());
		}
		rompath.append(fileSelected);
	} else if (nav == FS_DIR_BACK){
		rompath = listMenu.listDir.dir + listMenu.listDir.getRelativePath();
	}
	LOG_DEBUG("rompath: %s", rompath.c_str());
	
	if (dir.isDir(rompath.c_str())){
		vector<unique_ptr<FileProps>> files;
		ConfigEmu *emu = cfgLoader->getCfgEmu();
		string extFilter = " " + emu->rom_extension;
		extFilter = Constant::replaceAll(extFilter, " ", ".");
		dir.listFiles(rompath.c_str(), files, extFilter, "", emu->menu_show_directories, false, false);
		listMenu.filesToList(files, *emu);
		if (nav == FS_DIR_CD){
			listMenu.listDir.addRelativePath(fileSelected);
		}
		ret = FS_DIR_NAVIGATION;
	} else {
		listMenu.listDir.file = fileSelected;
		ret = FS_DIR_ISFILE;
	}

	if (ret == FS_DIR_NAVIGATION){
		//Cambiamos el tamanyo del listado para poder mostrar la ruta relativa de directorios
		listMenu.resizeMarginTop(face_h_big, overlay->h);
	}
	return ret;
}

std::string GameMenu::GetMD5(const std::string& input)
{
    md5_state_t state;
    md5_byte_t  digest[16];   // 128 bits = 16 bytes

    md5_init(&state);
    md5_append(&state,
               reinterpret_cast<const md5_byte_t*>(input.c_str()),
               (int)input.size());
    md5_finish(&state, digest);

    // Convertir los 16 bytes a cadena hexadecimal (32 caracteres)
    char hex[33];
    for (int i = 0; i < 16; ++i)
        sprintf_s(hex + i*2, 3, "%02x", digest[i]);   // sprintf_s para VS2010
    hex[32] = '\0';

    return std::string(hex);
}

/**
* Extracts a compressed file from the zip file, and sets it's filepath on "listMenu.listZipped.extractedFile" variable. 
* Returns FS_ZIP_FILE_EXTRACTED on success or FS_ZIP_EXTRACT_ERROR on failure
*/

FILE_STATUS GameMenu::extractFileFromZip(const std::string& internalPath, const std::string& extractionPath, ZipBrowser& zb, ListMenu &listMenu){
	LOG_DEBUG("extrayendo %s...", internalPath.c_str());
	LOG_DEBUG("...en %s", extractionPath.c_str());
	if (zb.ExtractFile(internalPath, extractionPath)){
		//Establecemos cual es el nombre del fichero extraido en el filesystem
		listMenu.listZipped.extractedFile = extractionPath;
		return FS_ZIP_FILE_EXTRACTED;
	} else {
		return FS_ZIP_EXTRACT_ERROR;
	}
}


/**
 * 
 */
int GameMenu::saveGameMenuPos(ListMenu &menuData){
    FILE* outfile;
    string filepath = Constant::getAppDir() + Constant::getFileSep() + MENUTMP;
    int ret = 0;
    // open file for writing
    outfile = fopen(filepath.c_str(), "wb");
    if (outfile == NULL) {
        LOG_ERROR("Error Writing to File: %s", filepath.c_str());
        return 1;
    }

    struct ListStatus input1(cfgLoader->emuCfgPos, menuData.iniPos, menuData.endPos, 
		menuData.curPos, menuData.maxLines, menuData.layout, menuData.animateBkg, 
		menuData.gameDataFields.posManufacturer, menuData.gameDataFields.posSystem, 
		menuData.gameDataFields.posYear, menuData.gameDataFields.onlyParents);

	//Guardando los datos si se ha seleccionado un fichero zip
	strcpy_s(input1.zipname, sizeof(input1.zipname), (menuData.listZipped.dir + Constant::getFileSep() + menuData.listZipped.file).c_str());
	strcpy_s(input1.zippedPath, sizeof(input1.zippedPath), menuData.listZipped.getInternalDir().c_str());
	//Guardando los datos cuando se selecciona un directorio relativo al directorio de roms del emulador
	strcpy_s(input1.relativePath, sizeof(input1.relativePath), menuData.listDir.getRelativePath().c_str());

    int flag = 0;
    flag = fwrite(&input1, sizeof(struct ListStatus), 1, outfile);

    if (flag) {
        LOG_DEBUG("Contents of the structure written successfully");
    } else {
        LOG_ERROR("Error Writing to File: %s", filepath.c_str());
        ret = 1;
    }
    fclose(outfile);
    return ret;
}

/**
 * 
 */
int GameMenu::recoverGameMenuPos(ListMenu &menuData, struct ListStatus &read_struct){
    FILE* infile;
    string filepath = Constant::getAppDir() + Constant::getFileSep() + MENUTMP;
    int ret = 0;

    // Open person.dat for reading
    infile = fopen(filepath.c_str(), "rb");
    if (infile == NULL) {
        cerr << "Error openning file: " << filepath << endl;
        return 1;
    }

    if (fread(&read_struct, sizeof(read_struct), 1, infile) > 0){
        LOG_DEBUG("emupos: %d; inipos: %d; endpos: %d; curpos: %d; maxlines: %d; layout: %d; animateBkg: %d", read_struct.emuLoaded,  
			read_struct.iniPos, read_struct.endPos, read_struct.curPos, read_struct.maxLines, read_struct.layout, read_struct.animateBkg);
        //Setting the emulator selected        
        cfgLoader->emuCfgPos = read_struct.emuLoaded;
    } else {
        ret = 1;
    }

    fclose(infile);
    return ret;
}

bool GameMenu::updateFps(){
	bool shouldUpdateFps = false;
	TTF_Font *font = Fonts::getFont(Fonts::FONTSMALL);

    if (*this->mustUpdateFps) {
        uint32_t currentTick = SDL_GetTicks();
        
        // Temporizadores independientes
        shouldUpdateFps = (currentTick - lastFpsUpdate > 500) || fpsSurface == NULL;
		//const bool shouldUpdateFps = (currentTick - lastFpsUpdate > 500);
        const bool shouldUpdateMem = (currentTick - lastMemUpdate > 5000) || memSurface == NULL;
		int lastCpuW = 0;

        // 1. Actualizacion de contadores internos
        this->sync->update_fps_counter(shouldUpdateFps, currentTick);
        // CPU utilization basada en GetThreadTimes sampling cada
        // frame (no solo cuando refrescamos el overlay) para que la
        // ventana movil capture la actividad real del thread.  El
        // valor suavizado queda en this->sync->utilization.
        this->sync->sample_cpu_utilization();

        // 2. Logica para FPS y CPU (cada 500ms)
        if (shouldUpdateFps) {
            if (fpsSurface) {
				SDL_FreeSurface(fpsSurface);
			}
            if (cpuSurface) {
				lastCpuW = cpuSurface->w;
				SDL_FreeSurface(cpuSurface);
			}
            _snprintf(this->sync->cpuText, sizeof(this->sync->cpuText), CPU_FORMAT, this->sync->utilization);
			//OutputDebugStringA(this->sync->cpuText);
			//OutputDebugStringA(" fps: ");
			_snprintf(this->sync->fpsText, sizeof(this->sync->fpsText), FPS_FORMAT, this->sync->g_actualFps);
			//OutputDebugStringA(this->sync->fpsText);
			//OutputDebugStringA("\n");
			
            fpsSurface = Fonts::renderUtf8Shaded(font, this->sync->fpsText, Constant::colors[clWhite].sdlColor, Constant::colors[clBlack].sdlColor);
            cpuSurface = Fonts::renderUtf8Shaded(font, this->sync->cpuText, Constant::colors[clWhite].sdlColor, Constant::colors[clBlack].sdlColor);
            
            lastFpsUpdate = currentTick;
        }

        // 3. Logica para MEMORIA (cada 5000ms / 5 segundos)
		if (shouldUpdateMem) {
			if (memSurface) SDL_FreeSurface(memSurface);
			
			double availMB = 0;
			double totalPercent = 0;
#ifdef _XBOX
			MEMORYSTATUS ms;
			ms.dwLength = sizeof(ms);
			GlobalMemoryStatus(&ms);
			availMB = (double)ms.dwAvailPhys / (1024.0 * 1024.0);
			// Calculo manual del porcentaje: (Usado / Total) * 100
			double totalPhys = (double)ms.dwTotalPhys;
			if (totalPhys > 0) {
				totalPercent = ((totalPhys - (double)ms.dwAvailPhys) / totalPhys) * 100.0;
			} else {
				totalPercent = 0.0;
			}
#else 
			MEMORYSTATUSEX ms;
			ms.dwLength = sizeof(ms); // <--- OBLIGATORIO para GlobalMemoryStatusEx
			GlobalMemoryStatusEx(&ms);
			availMB = (double)ms.ullAvailPhys / (1024.0 * 1024.0);
			totalPercent = ms.dwMemoryLoad;
#endif
			char memText[64];
			// Usamos double para mayor precision en los calculos de GB
			if (availMB >= 1024.0) {
				sprintf(memText, "MEM: %.0f%% (%.2fGB Free)", totalPercent, availMB / 1024.0);
			} else {
				sprintf(memText, "MEM: %.0f%% (%.0fMB Free)", totalPercent, availMB);
			}

			memSurface = Fonts::renderUtf8Shaded(font, memText, Constant::colors[clWhite].sdlColor, Constant::colors[clBlack].sdlColor);
			lastMemUpdate = currentTick;
		}

        // 4. DIBUJO (En cada frame, usando las superficies actuales)
        if (fpsSurface && cpuSurface && memSurface) {
            // Dibujar MEM (Posicion relativa a CPU)
            SDL_Rect rectMem = {this->overlay->w - memSurface->w -3, 1, memSurface->w, memSurface->h};
			clearOverlayRect(rectMem);
            SDL_BlitSurface(memSurface, NULL, this->overlay, &rectMem);

            // Dibujar CPU (Posicion relativa a FPS)
            SDL_Rect rectCpu = {rectFps.x, rectFps.y - fpsSurface->h, lastCpuW != cpuSurface->w ? lastCpuW : cpuSurface->w, cpuSurface->h};
            clearOverlayRect(rectCpu);
            SDL_BlitSurface(cpuSurface, NULL, this->overlay, &rectCpu);
			lastCpuW = cpuSurface->w;

			// Dibujar FPS
            SDL_FillRect(this->overlay, &rectFps, bkgTextFps);
            SDL_BlitSurface(fpsSurface, NULL, this->overlay, &rectFps);
        }
    }
	return shouldUpdateFps;
}

void GameMenu::processFrontendEvents(HOTKEYS_LIST hotkey){
	// Procesamos hotkeys
	processHotkeys(hotkey);
}

void GameMenu::processFrontendEventsAfter(){
	// Actualizamos el contador de media de fps
	updateFps();
	//Mostramos mensajes
	processMessages();
	//Mostramos mensajes de los logros
	processMessagesAchievements();
	//Actualizamos para detectar cuando soltamos un boton
	processKeyUp();
	if (isOnscreenKeybEnabled()){
		drawKeyboard(Fonts::getFont(Fonts::FONTSMALL), *keyb);
	}
}

void GameMenu::processKeyUp(){
	if (getEmuStatus() == EMU_STARTED) return;
	joystick->inputs.updateLastState();
}

/**
* Procesamos las hotkeys mientras el juego esta corriendo
*/
void GameMenu::processHotkeys(HOTKEYS_LIST hotkey){
	if (getEmuStatus() != EMU_STARTED) return;

	#ifndef SALVIA_GPU_VIDEO
	int modeOk = true;
	int startingMode = *this->current_scaler_mode;
	struct retro_system_av_info av_info;
	retro_get_system_av_info(&av_info);
	const unsigned ancho_base = av_info.geometry.base_width;
	const unsigned alto_base = av_info.geometry.base_height;
	#endif

	std::string msgShader;
	std::string choosenFilter;
	ConfigEmu *emu = getCfgLoader()->getCfgEmu();
	static int lastSync = *current_sync;

	switch (hotkey){
		case HK_RATIO:
			*this->current_ratio = (*this->current_ratio + 1) % TOTAL_VIDEO_RATIO;
			SDL_XBOX_SetDisplaySize(aspectRatioValues[*this->current_ratio]);
			showSystemMessage(aspectRatioStrings[*this->current_ratio], 3000);
			break;

		case HK_SHADER:
			#ifdef SALVIA_GPU_VIDEO
				*this->current_shader = (*this->current_shader + 1) % TOTAL_SHADERS;
				XBOX_SelectEffect(*this->current_shader);
				choosenFilter = "menu.video.shader" + Constant::TipoToStr(*this->current_shader);
				msgShader = LanguageManager::instance()->get("msg.filter") + " " 
					+ LanguageManager::instance()->get(choosenFilter);
				showSystemMessage(msgShader, 3000);
			#else
				do {
					*this->current_scaler_mode = ((*this->current_scaler_mode + 1) % TOTAL_VIDEO_SCALE);

					const int dw = this->overlay->w;
					const int dh = this->overlay->h;
					bool cannotScale2x = (*current_scaler_mode == SCALE2X || *current_scaler_mode == SCALE_HQ2X_ALT //|| *current_scaler_mode == SCALE_HQ2X 
						|| *current_scaler_mode == SCALE_XBRZ_2X 
						|| *current_scaler_mode == SCALE_XBRZ_2X_TH) && ((int)ancho_base * 2 > dw || (int)alto_base * 2 > dh);
					bool cannotScale3x = (*current_scaler_mode == SCALE3X || *current_scaler_mode == SCALE3X_ADV 
						|| *current_scaler_mode == SCALE_HQ3X_ALT || *current_scaler_mode == SCALE_XBRZ_3X 
						|| *current_scaler_mode == SCALE_XBRZ_3X_TH) && ((int)ancho_base * 3 > dw || (int)alto_base * 3 > dh);
					bool cannotScale4x = (*current_scaler_mode == SCALE4X || *current_scaler_mode == SCALE4X_ADV 
						|| *current_scaler_mode == SCALE_XBRZ_4X) && ((int)ancho_base * 4 > dw || (int)alto_base * 4 > dh);

					#ifdef _XBOX
						//XBOX CPU can't handle this algorithm implementations at 60fps
						cannotScale2x = cannotScale2x || *current_scaler_mode == SCALE_XBRZ_2X  || *current_scaler_mode == SCALE_XBRZ_2X_TH || *current_scaler_mode == SCALE_HQ2X_ALT; //|| *current_scaler_mode == SCALE_HQ2X;
						cannotScale3x = cannotScale3x || *current_scaler_mode == SCALE_XBRZ_3X || *current_scaler_mode == SCALE_XBRZ_3X_TH || *current_scaler_mode == SCALE_HQ3X_ALT;
						cannotScale4x = cannotScale4x || *current_scaler_mode == SCALE_XBRZ_4X;
					#endif

					if (cannotScale2x || cannotScale3x || cannotScale4x || *current_scaler_mode == NO_VIDEO){
						modeOk = false;
					} else if (*this->current_integer_scale == false && (*current_scaler_mode == SCALE4X || *current_scaler_mode == SCALE3X 
								|| *current_scaler_mode == SCALE2X || *current_scaler_mode == SCALE1X)) {
						//Si queremos pantalla completa, no tiene sentido pasemos por un scalenx.
						modeOk = false;
					} else {
						modeOk = true;
					}
				} while(!modeOk && *current_scaler_mode != startingMode);

				LOG_INFO("scaler %d - %s\n", *current_scaler_mode, videoScaleStrings[*current_scaler_mode].c_str());

				selectScalerMode(*current_scaler_mode);
				SDL_FillRect(this->overlay, NULL, this->uBkgColor);
				showSystemMessage(videoScaleStrings[*current_scaler_mode], 3000);
			#endif
			break;
		case HK_EXIT_GAME:
			if (!Achievements::instance()->canPause()){
				showLangSystemMessage(LanguageManager::instance()->get("msg.error.hardcore.pause"), 3000);
				break;
			} 

			if (bg_screenshot){
				SDL_FreeSurface(bg_screenshot);
				bg_screenshot = NULL;
			}
			bg_screenshot = clonarPantalla(gameScreen, 180);
			fillOverlay(clBackground);
			//fillOverlay(clBlack);
			//clearOverlay();
			setEmuStatus(EMU_MENU);
			break;
		case HK_VIEW_MENU:
			if (!Achievements::instance()->canPause()){
				showLangSystemMessage(LanguageManager::instance()->get("msg.error.hardcore.pause"), 3000);
				break;
			} 

			setEmuStatus(getEmuStatus() == EMU_MENU_OVERLAY ? getLastStatus() : EMU_MENU_OVERLAY);
			if (bg_screenshot){
				SDL_FreeSurface(bg_screenshot);
				bg_screenshot = NULL;
			}

			if (getEmuStatus() == EMU_MENU_OVERLAY && overlay){
				bg_screenshot = clonarPantalla(gameScreen, 180);
				//Si hay mensajes de logros en curso, mostramos el menu de logros
				if (!messagesAchievement.empty() && messagesAchievement.get_at(0)->type == ACH_UNLOCKED){
					configMenus->setAchievementsAsSelected();
					configMenus->descargarLogros();
				} else if (!configMenus->obtenerMenuActual()->opciones.empty()){
					auto option = configMenus->obtenerMenuActual()->opciones.front();
					if (option->tipo == OPC_ACHIEVEMENT){
						configMenus->descargarLogros();
					} else if (option->tipo == OPC_SAVESTATE){
						configMenus->poblarPartidasGuardadas(getCfgLoader(), romPaths.rompath);
					}
				}
			}
			break;
		case HK_ONSCREEN_KEYB:
			if (!emu->keyboard_type.empty()){
				setOnscreenKeyboard(!isOnscreenKeybEnabled());
				if (!isOnscreenKeybEnabled()){
					SDL_Rect rectMem = {keyb->iniX, keyb->iniY, keyb->keyboardW + 1, keyb->keyboardH + 1};
					clearOverlayRect(rectMem);
				}
			}
			break;
		case HK_FAST_FORWARD:
			if (!Achievements::instance()->isHardcoreMode()){
				const float speed = CfgLoader::configMain[cfg::fastForwardMult].valueInt / (float)10;
				if (speed == 0){
					showLangSystemMessage("msg.fastfoward.speed.wrong", 3000);
					break;
				}
				const float originalFps = (float)getAvInfo().timing.fps;
				current_fast_forward = !current_fast_forward;
				SDL_XBOX_SetVSync(current_fast_forward ? 0 : 1);
				if (current_fast_forward){
					sync->init_fps_counter(originalFps * speed);
					lastSync = *current_sync;
					*current_sync = SYNC_TO_VIDEO;
					showSystemMessage(Constant::string_format(LanguageManager::instance()->get("msg.fastfoward.speed"), speed * 100), 3000);
				} else {
					sync->init_fps_counter(originalFps);
					*current_sync = lastSync;
					showLangSystemMessage("msg.fastfoward.reset", 3000);
				}
			} else {
				showLangSystemMessage("msg.fastfoward.hardcore", 3000);
			}
			break;
	}
}

void GameMenu::setEmuStatus(int tmpStat){
	if (status == EMU_MENU_IMAGE_VIEWER){
		//No queremos volver al visor de imagenes
		lastStatus = EMU_MENU;
	} else {
		lastStatus = status;
	}
	status = tmpStat;
	//Fondo HLSL del menu: estado retenido decidido en cada transicion
	applyMenuBackground();
	//Siempre que cambiemos de estado de emulacion,
	//reseteamos los botones del joystick
	joystick->inputs.clearAll();

	//Deshabilitamos el teclado si lo estabamos mostrando en el core
	if (lastStatus == EMU_STARTED && status != EMU_STARTED && isOnscreenKeybEnabled()){
		setOnscreenKeyboard(false);
	}

	if (status == EMU_STARTED && lastStatus != EMU_STARTED){
		BadgeDownloader::instance().stop();
		//Restauramos el shader porque parece haber algun problema con HLSLBackground::draw
		checkDisplayOptions();
	}
}

struct retro_system_av_info GameMenu::getAvInfo(){
	struct retro_system_av_info av_info;
	memset(&av_info, 0, sizeof(av_info));
	retro_get_system_av_info(&av_info);
	return av_info;
}

/**
*
*/
SDL_Surface* GameMenu::clonarPantalla(SDL_Surface* src, int transparency) {
    if (!src) return NULL;
	SDL_Surface* copia = NULL;
	//Dimensiones y calculo de aspecto
	Dimension srcDim = {src->w, src->h};
	Dimension dstDim = {overlay->w, overlay->h};
	Dimension resDim = Image::relacion(srcDim, dstDim);
	Dimension resCen = Image::centrado(resDim, dstDim);
	SDL_Rect dstRect = {resCen.w , resCen.h, (Uint16)resDim.w, (Uint16)resDim.h};

	if (resDim.w == src->w && resDim.h == src->h){
		copia = SDL_CreateRGBSurface(SDL_SWSURFACE, overlay->w, overlay->h, overlay->format->BitsPerPixel, 
			overlay->format->Rmask, overlay->format->Gmask, 
			overlay->format->Bmask, 0);
		SDL_BlitSurface(src, NULL, copia, NULL);
		boxRGBA(copia, 0, 0, copia->w -1, copia->h -1, Constant::colors[clBackground].sdlColor.r, Constant::colors[clBackground].sdlColor.g, Constant::colors[clBackground].sdlColor.b, transparency);
	} else {
		SDL_Surface *tmp = SDL_CreateRGBSurface(SDL_SWSURFACE, overlay->w, overlay->h, 
			src->format->BitsPerPixel, src->format->Rmask, src->format->Gmask, 
			src->format->Bmask, src->format->Amask);
		
		double zoomX = (double)dstRect.w / srcDim.w;
		double zoomY = (double)dstRect.h / srcDim.h;
		SDL_Surface *tmp2 = zoomSurface(src, zoomX, zoomY, false);
		SDL_BlitSurface(tmp2, NULL, tmp, &dstRect);
		SDL_FreeSurface(tmp2);

		if (transparency < 255) {
			boxRGBA(tmp, 0, 0, tmp->w - 1, tmp->h - 1, 
					Constant::colors[clBackground].sdlColor.r, 
					Constant::colors[clBackground].sdlColor.g, 
					Constant::colors[clBackground].sdlColor.b, 
					transparency);
		}
		copia = SDL_ConvertSurface(tmp, overlay->format, overlay->flags);
		//copia = SDL_DisplayFormat(tmp);
		SDL_FreeSurface(tmp);
	}
	
	// Imagen plana sin transparencias
	SDL_SetAlpha(copia, SDL_RLEACCEL, 0xFF);
    return copia;
}

void GameMenu::selectScalerMode(int mode){
#if !defined(_XBOX) && !defined(SALVIA_GPU_VIDEO)
	// 3. Selector de escalado
	switch(mode) {
		case FULLSCREEN:
				#ifdef WIN
					//Tests made with Genesis Sonic 1, idle on the first stage 
					//current_scaler = scale_software_fixed_point;				//520fps
					current_scaler = scale_software_fixed_point_safe2;		    //508fps	
					//current_scaler = scale_software_fixed_point_simple_safe;  //490fps
					//current_scaler = scale_software_fixed_point_sse2_safe;    //490fps
					//current_scaler = scale_software_fixed_point_simple;       //450fps
					//current_scaler = scale_software_float_sse;		        //430fps
					//current_scaler = scale_software_fixed_point_notif;        //400fps
					//current_scaler = scale_software_fixed_point_x86_simd;	    //380fps	
					//current_scaler = scale_software_fixed_point_noif_x86;	    //360fps
					//current_scaler = scale_software_float;				    //265fps
				#elif defined(_XBOX)
					current_scaler = scale_software_fixed_point_xbox_final; //112fps
				#else
					current_scaler = scale_software_fixed_point_safe2;		//106fps
				#endif
			break;

		case SCALE1X:
			#ifdef WIN
				current_scaler = fast_video_blit;
			#elif defined(_XBOX)
				current_scaler = fast_video_blit_xbox;
			#endif
			break;

		case SCALE2X:
			current_scaler = scale2x_software;
			break;

		case SCALE3X:
			current_scaler = scale3x_software;
			break;

		case SCALE4X:
			current_scaler = scale4x_software;
			break;

		case SCALE2X_ADV:
			current_scaler = scale2x_advance;
			break;

		case SCALE3X_ADV:
			current_scaler = scale3x_advance;
			break;

		case SCALE4X_ADV:
			current_scaler = scale4x_advance;
			break;

		case SCALE_HQ2X_ALT:
			#ifdef _XBOX
			current_scaler = scale_hqnx_alt;
			#else
			current_scaler = scale_hq2x_xbox;
			#endif
			current_scaler_scale = 2;
			break;

		case SCALE_HQ3X_ALT:
			current_scaler = scale_hqnx_alt;
			current_scaler_scale = 3;
			break;

		case SCALE_XBRZ_2X: 
			current_scaler = scale_xBRZ_nx;
			current_scaler_scale = 2;
			break;

		case SCALE_XBRZ_2X_TH:
			current_scaler = xbrz_scale_multithread;
			current_scaler_scale = 2;
			break;

		case SCALE_XBRZ_3X: 
			current_scaler = scale_xBRZ_nx;
			current_scaler_scale = 3;
			break;

		case SCALE_XBRZ_3X_TH: 
			current_scaler = xbrz_scale_multithread;
			current_scaler_scale = 3;
			break;

		case SCALE_XBRZ_4X: 
			current_scaler = scale_xBRZ_nx;
			current_scaler_scale = 4;
			break;

		case NO_VIDEO:
			current_scaler = no_video;
			break;

		default:
			current_scaler = scale_software_fixed_point_safe2;
			break;
	}
#endif
}

void GameMenu::showLangSystemMessage(std::string text, uint32_t duration) {
	showSystemMessage(LanguageManager::instance()->get(text), duration);
}

void GameMenu::showSystemMessage(std::string text, uint32_t duration) {

	if (text.empty() || duration == 0) return;

    Message msg;
    msg.content = text;
    msg.timeout = duration;
    msg.ticks = SDL_GetTicks();
    
    std::string newText = Fonts::recortarAlTamanyo(text, this->overlay->w);
	msg.cache = Fonts::renderUtf8Blended(Fonts::getFont(Fonts::FONTBIG), newText.c_str(), Constant::colors[clWhite].sdlColor);
    
    if (msg.cache) {
        msg.rect.x = 0;
        // La posicion Y se calculara dinamicamente al dibujar para que se apilen
        msg.rect.w = msg.cache->w + 2;
        msg.rect.h = face_h_big + 4;
        messages.push_back(msg);
    }
}

void GameMenu::renderTrackers() {
    Achievements* ach = Achievements::instance();
    if (ach->trackers.empty()) return;

    TTF_Font* font = Fonts::getFont(Fonts::FONTSMALL);
	int posX = rectFps.x;
    int posY = 5;

	if (*this->mustUpdateFps) {
		posY += rectFps.h +  rectFps.y;
	}

    ach->trackers.render(this->overlay, font, posX, posY);
}


void GameMenu::renderChallenges() {
    Achievements* ach = Achievements::instance();
    if (ach->challenges.empty()) return;
	ach->challenges.render(this->overlay, 0);
}

void GameMenu::renderProgress() {
	Achievements::instance()->progress.render(overlay, 0, Constant::colors[clBackground]);
}

void GameMenu::processMessagesAchievements(){
	if (!getCfgLoader()->configMain[cfg::enableAchievements].valueBool)
		return;

	const uint32_t currentTicks = SDL_GetTicks();

	// 2. Solo procesar si realmente ha pasado tiempo (Throttle)
    // No necesitas actualizar la logica de logros cada 16ms (60fps). 
    // Hacerlo cada 33ms o 100ms libera muchisima CPU.
    static uint32_t lastUpdate = 0;
    if (currentTicks - lastUpdate > 33) { 
		// Actualizar estado interno de los logros
		updateAchievementsState(currentTicks);
		// Gestion de la cola de mensajes (Expiracion y carga)
		handleMessageQueue(currentTicks);
		lastUpdate = currentTicks;
	}
    
	// 3. Renderizado condicional: Solo entra si hay algo que dibujar
	// Renderizado (Si hay mensajes)
	if (!messagesAchievement.empty()) {
		renderCurrentAchievement();
	}
    
	// Renderizado de trackers
	renderTrackers();
	// Renderizado de challenges
	renderChallenges();
	// Renderizado de progresos
	renderProgress();
}

inline void GameMenu::updateAchievementsState(uint32_t currentTicks) {
    if (getEmuStatus() == EMU_STARTED) {
		Achievements* ach = Achievements::instance();
        ach->doFrame();
        // Load new messages to the list
		while (!ach->messages.empty()) {
			AchievementState msg;
			if (ach->messages.pop_with_new_surfaces(msg, this->overlay->format)){
				messagesAchievement.add(msg);
			}
			//Liberamos para que no se haga un doble free por si acaso
			msg.badge = NULL;
			msg.badgeLocked = NULL;
		}
    } else {
        // 1second logic when we are on the emulator's menu
        static uint32_t lastIdleTick = 0;
        if (currentTicks - lastIdleTick > 1000) {
            Achievements::instance()->doIdle();
            lastIdleTick = currentTicks;
        }
    }
}

inline void GameMenu::handleMessageQueue(uint32_t currentTicks) {
    if (messagesAchievement.empty()) return;

    // Obtenemos referencia al primer mensaje
	AchievementState* ach = messagesAchievement.get_at(0);
    
    if (ach->ticks == 0) {
        messagesAchievement.update_ticks(currentTicks); // Iniciar temporizador
    } else if (currentTicks - ach->ticks > ach->timeout) {
		//Limpiamos el ultimo mensaje
		ach->clearSurfaces();
		AchievementState msg;
        messagesAchievement.pop(msg);
		clearLastAchievementArea();
    }
}


void GameMenu::clearLastAchievementArea() {
    // Solo limpiamos si el area tiene dimensiones validas
	if (achievement_surface.lastPos.w > 0 && achievement_surface.lastPos.h > 0) {
        // Pintamos un rectungulo del color de fondo (uBkgColor) 
        // sobre el area que ocupaba el ultimo logro
		clearOverlayRect(achievement_surface.lastPos);
    }

	if (achievement_surface.srf){
		SDL_FreeSurface(achievement_surface.srf);
		achievement_surface.srf = NULL;
	}
}

void GameMenu::renderCurrentAchievement() {
	if (messagesAchievement.empty())
		return;

    AchievementState* msg = messagesAchievement.get_at(0); 

	if (msg->type == ACH_LOAD_GAME) {
        showAchievementMessage(Constant::string_format(LanguageManager::instance()->get("msg.achievement.loaded.title"), msg->title.c_str()), 
							   Constant::string_format(LanguageManager::instance()->get("msg.achievement.loaded.points"), msg->achvTotal, msg->scoreTotal), 
							   Constant::string_format(LanguageManager::instance()->get("msg.achievement.loaded.unlocked"), msg->achvUnlocked), 
                               msg->badge);
    } else if (msg->type == ACH_UNLOCKED){
		Achievements::instance()->setShouldRefresh(true);
        showAchievementMessage(LanguageManager::instance()->get("msg.achievement.unlocked.title"), msg->title, msg->description, msg->badge);
    } else if (msg->type == ACH_WARNING){
        showAchievementMessage(LanguageManager::instance()->get("msg.achievement.warning.title"), msg->title, msg->description, msg->badge);
    }
}

void GameMenu::showAchievementMessage(const std::string &line1Str, const std::string &line2Str, const std::string &line3Str, SDL_Surface *badge){

	if (achievement_surface.srf == NULL){
		SDL_Surface *line1 = Fonts::renderUtf8Blended(Fonts::getFont(Fonts::FONTSMALL), line1Str.c_str(), Constant::colors[clWhite].sdlColor);
		SDL_Surface *line2 = Fonts::renderUtf8Blended(Fonts::getFont(Fonts::FONTSMALL), line2Str.c_str(), Constant::colors[clYellow].sdlColor);
		SDL_Surface *line3 = Fonts::renderUtf8Blended(Fonts::getFont(Fonts::FONTSMALL), line3Str.c_str(), Constant::colors[clBlue].sdlColor);

		const int paddingBottom = 10;
		int maxW = line1->w > line2->w ? line1->w : line2->w;
		maxW = maxW > line3->w ? maxW : line3->w; 
		int line_height, badgeW, badgeH, badgePad;
		Fonts::getBadgeSize(badgeW, badgeH, badgePad, line_height);
		maxW = paddingBottom + maxW + badgePad * 3 > overlay->w ? (paddingBottom + maxW + badgePad * 3) - overlay->w : maxW;
		const int maxH = this->overlay->h -paddingBottom -line_height * 3;
		SDL_Rect rect = {10 + badgeW, 0, 0, line_height};
		achievement_surface.pos.x = rect.x - badgeW;
		achievement_surface.pos.y = maxH;
		achievement_surface.pos.w = maxW + badgeW + badgePad * 3;
		achievement_surface.pos.h = this->overlay->h - maxH - paddingBottom;

		Achievements& self = *Achievements::instance();

		//Limpiamos el espacio ocupado por la superficie anterior
		clearLastAchievementArea();
		achievement_surface.lastPos = achievement_surface.pos;
		//La creamos de nuevo
		achievement_surface.srf = SDL_CreateRGBSurface(SDL_SWSURFACE, achievement_surface.pos.w, achievement_surface.pos.h, 
															overlay->format->BitsPerPixel,
															overlay->format->Rmask, 
															overlay->format->Gmask, 
															overlay->format->Bmask, 
															overlay->format->Amask);

		if (achievement_surface.srf == NULL)
			return;

		SDL_FillRect(achievement_surface.srf, NULL, Constant::colors[clPaleBlue].color);
		SDL_SetAlpha(achievement_surface.srf, SDL_RLEACCEL, 0xFF);

		SDL_Rect txtRect = {badgeW + badgePad * 2, 0, line1->w, line_height};
		SDL_BlitSurface(line1, NULL, achievement_surface.srf, &txtRect);

		txtRect.y = line_height;
		txtRect.w = line2->w;
		SDL_BlitSurface(line2, NULL, achievement_surface.srf, &txtRect);

		txtRect.y = line_height * 2;
		txtRect.w = line3->w;
		SDL_BlitSurface(line3, NULL, achievement_surface.srf, &txtRect);
	
		SDL_FreeSurface(line1);
		SDL_FreeSurface(line2);
		SDL_FreeSurface(line3);

		if (badge != NULL){
			SDL_Rect rectBadge = {badgePad, badgePad, 0, 0};
			SDL_BlitSurface(badge, NULL, achievement_surface.srf, &rectBadge);
		}
	}

	if (achievement_surface.srf != NULL){
		SDL_BlitSurface(achievement_surface.srf, NULL, overlay, &achievement_surface.pos);
	}
}

/**
*
*/
void GameMenu::processMessages() {
	//Se anyaden mensajes recibidos desde achievements para el login
	Achievements& ach = *Achievements::instance();
	//EnterCriticalSection(&ach.m_csMessages);
	while (!ach.messagesToInform.empty()){
		// 1. Obtener el mensaje mas antiguo (el primero que entro)
		showSystemMessage(ach.messagesToInform.front(), 3000);
		// 2. Eliminarlo del deque de forma definitiva
		ach.messagesToInform.pop_front();
	}
	//LeaveCriticalSection(&ach.m_csMessages); // Liberamos el candado inmediatamente

    if (messages.empty()) return;

    // 1. LIMPIEZA TOTAL: Antes de mover nada, borramos la zona donde suelen estar
    // (Opcional: puedes calcular un rect global que cubra todos los mensajes)
    for (std::size_t i = 0; i < messages.size(); ++i) {
        clearOverlayRect(messages[i].rect); 
    }

    // 2. ACTUALIZACION: Eliminar mensajes caducados
    uint32_t currentTicks = SDL_GetTicks();
    for (int i = (int)messages.size() - 1; i >= 0; i--) {
        if (currentTicks - messages[i].ticks > messages[i].timeout) {
            if (messages[i].cache) SDL_FreeSurface(messages[i].cache);
            messages.erase(messages.begin() + i);
        }
    }

    if (messages.empty()) return;

    // 3. CALCULO DE POSICIONES Y DIBUJO
    static int line_height = face_h_big + 4;
    int currentY = this->overlay->h - line_height;

    // Usamos referencia directa en el bucle para mayor seguridad que el puntero mData
    for (int i = (int)messages.size() - 1; i >= 0; --i) {
        Message &m = messages[i];
        
        // Actualizamos la nueva posicion
        m.rect.x = 0;
        m.rect.y = (Sint16)currentY;

        if (m.cache) {
            m.rect.w = (Uint16)m.cache->w;
            m.rect.h = (Uint16)m.cache->h;
            
            // Dibujamos fondo y texto
            SDL_FillRect(overlay, &m.rect, Constant::colors[clBackground].color);
            SDL_BlitSurface(m.cache, NULL, this->overlay, &m.rect);
        }
        currentY -= line_height;
    }
}

void GameMenu::processConfigChanges(){
	selectScalerMode(*this->current_scaler_mode);
}

void GameMenu::setRomPaths(std::string rp){
	dirutil dir;
	romPaths.rompath = rp;
	const std::string coreName = getCfgLoader()->configMain[cfg::libretro_core].valueStr;
	std::string statesDir = getCfgLoader()->configMain[cfg::libretro_state].valueStr + Constant::getFileSep() + coreName;
			
	if (!dir.dirExists(statesDir.c_str())){
		dir.createDirRecursive(statesDir.c_str());
	}

	romPaths.savestate = statesDir + Constant::getFileSep() + 
		dir.getFileNameNoExt(rp) + STATE_EXT;

	std::string sramDir = getSramPath();
	romPaths.sram = sramDir + Constant::getFileSep() + 
		dir.getFileNameNoExt(rp) + ".srm";

	// Cheats: <path_prefix>\data\cheats\<core>\<rom>.cht (formato RetroArch). path_prefix
	// ya termina en separador. El usuario deja aqui sus .cht importados.
	std::string cheatsDir = getCfgLoader()->configMain[cfg::path_prefix].valueStr + "data" + Constant::getFileSep() + "cheats" +
		Constant::getFileSep() + coreName;
	if (!dir.dirExists(cheatsDir.c_str())){
		dir.createDirRecursive(cheatsDir.c_str());
	}
	romPaths.cht = cheatsDir + Constant::getFileSep() + dir.getFileNameNoExt(rp) + ".cht";

	//Loading the joystick configuration if exists
	std::string ruta = dir.getFolder(rp) + Constant::getFileSep() + dir.getFileNameNoExt(rp) + CFG_JOY_EXT;

	std::string coreDefaultsPath = Constant::getAppDir() + std::string(Constant::tempFileSep) + "config"
		+ std::string(Constant::tempFileSep) + PREFIX_DEFAULTS + coreName + CFG_JOY_EXT;

	if (dir.fileExists(ruta.c_str())){
		//Primero comprobamos si existe el fichero de configuracion del joystick en la ruta del juego
		joystick->loadButtonsRetro(ruta);
	} else if (dir.fileExists(coreDefaultsPath.c_str())){
		//Comprobamos si existe en la ruta de configuracion del core
		joystick->loadButtonsRetro(coreDefaultsPath);
	} else {
		//Finalmente y en ultima instancia, comprobamos si existe en la ruta del .ini y lo cargamos
		std::string rutaIni = Constant::getAppDir() + Constant::getFileSep() + RETROPAD_INI;
		joystick->loadButtonsRetro(rutaIni);
	}
}

std::string GameMenu::getSramPath(){
	dirutil dir;
	std::string sramDir = getCfgLoader()->configMain[cfg::libretro_save].valueStr + Constant::getFileSep() +
		getCfgLoader()->configMain[cfg::libretro_core].valueStr;
			
	if (!dir.dirExists(sramDir.c_str())){
		dir.createDirRecursive(sramDir.c_str());
	}
	return sramDir;
}

void GameMenu::startScrapping(){
	LOG_DEBUG("Starting the scrap process");
	if (Scrapper::isScrapping()){
		showLangSystemMessage("msg.scrapinprogress", 3000);
		return;
	}
	ScrapperConfig config;
	
	config.lenguaPreferida = cfgLoader->configMain[cfg::scrapLang].valueStr;
	config.regionPreferida = cfgLoader->configMain[cfg::scrapRegion].valueStr;
	config.origin = static_cast<SCRAP_FROM>(cfgLoader->configMain[cfg::scrapOrigin].valueInt);
	config.scrapArtType = static_cast<SCRAP_GAMES>(this->configMenus->getScrapGamesSelection());
	config.apiKeyTGDB = cfgLoader->configMain[cfg::apikeytgdb].valueStr;

	LOG_DEBUG("Seleccionando lengua %s y region %s", config.lenguaPreferida.c_str(), config.regionPreferida.c_str());
	SafeDownloadQueue dwQueue;
	int totalGames = 0;
	std::vector<ConfigEmu> emuThreadedScrapper;

	for (std::size_t i=0; i < cfgLoader->emulators.size() - 1; i++){
		if (this->configMenus->scrapSelection[i].selected){
			int idxEmu = this->configMenus->scrapSelection[i].index;
			if (idxEmu >= 0 && (std::size_t)idxEmu < cfgLoader->emulators.size() - 1){
				LOG_DEBUG("Scrapping system list %s", this->configMenus->scrapSelection[idxEmu].name.c_str());
				emuThreadedScrapper.push_back(cfgLoader->emulators[idxEmu].get()->config);
				totalGames += scrapper.scrapSystem(cfgLoader->emulators[idxEmu].get()->config, config, dwQueue, true);
			}
		}
	}
	Scrapper::g_status.total = totalGames;
	LOG_DEBUG("Total of games to scrap: %d", totalGames);
	if (emuThreadedScrapper.size() > 0){
		Scrapper::StartScrappingAsync(emuThreadedScrapper, config);
	}
}

void GameMenu::clearOverlay(){
	memset(overlay->pixels, 0, overlay->pitch * overlay->h); 
}

void GameMenu::clearOverlayRect(SDL_Rect& rect){
	SDL_FillRect(overlay, &rect, 0);
}

void GameMenu::fillOverlay(int colorIndex){
	if (colorIndex < clTotalColors){
		SDL_FillRect(this->overlay, NULL, Constant::colors[colorIndex].color);
	}
}

void GameMenu::fillOverlayAlpha(int colorIndex, int alpha){
	if (colorIndex < clTotalColors){
		const SDL_Color& col = Constant::colors[colorIndex].sdlColor;
		const Uint32 colorA = SDL_MapRGBA(this->overlay->format, col.r, col.g, col.b, alpha);
		SDL_FillRect(this->overlay, NULL, colorA);
	}
}

void GameMenu::drawSelectedKey(TTF_Font* font, t_keyboard& keyb, int row, int col){
	// 3. REMARCADO DE LA TECLA SELECCIONADA (En tiempo real)
    if (row >= 0 && row < keyb.rows &&
        col >= 0 && col < keyb.cols) 
    {
        // Para saber la posicion X exacta de la tecla seleccionada, 
        // sumamos los anchos de las teclas anteriores en su misma fila
        int targetX = keyb.iniX;
        for (int c = 0; c < col; c++) {
            if (keyb.layoutWidth[row][c] == 0) continue; // Ignoramos celdas absorbidas
            
            // Si la celda es un salto por culpa del ENTER vertical, sumamos el ancho que le corresponderia
            if (keyb.caps[row][c].h == 0) {
                int actualW = (keyb.layoutWidth[row][c] * keyb.keyW) + ((keyb.layoutWidth[row][c] - 1) * keyb.spaceX);
                targetX += actualW + keyb.spaceX;
            } else {
                targetX += keyb.caps[row][c].w + keyb.spaceX;
            }
        }

        int targetY = keyb.iniY + row * (keyb.keyH + keyb.spaceY);
        int targetW = keyb.caps[row][col].w;
        int targetH = keyb.caps[row][col].h;

        // Solo dibujamos si es una tecla valida y visible
        if (targetW > 0 && targetH > 0) {
            // Pintar un fondo encima de otro color (ej: Amarillo semitransparente)
            //boxRGBA(gameMenu->overlay, targetX, targetY, targetX + targetW, targetY + targetH, 255, 255, 0, 220);
			static bool colorChange = false;
			if (keyb.caps[row][col].blinkingPeriod == 0){
				colorChange = false;
			} else if (keyb.caps[row][col].blinkingPeriod-- % (TOTAL_BLINK_FRAMES / 3) == 0){
				colorChange = !colorChange;
			}

			SDL_Rect rect = {targetX, targetY, targetW, targetH};
			if (!colorChange){
				//roundedBoxRGBA(this->overlay, targetX, targetY, targetX + targetW, targetY + targetH, 5, 255, 255, 0, 220);
				SDL_FillRect(this->overlay, &rect, SDL_MapRGB(overlay->format, 255, 255, 0));
			} else {
				//roundedBoxRGBA(this->overlay, targetX, targetY, targetX + targetW, targetY + targetH, 5, 255, 128, 0, 255);
				SDL_FillRect(this->overlay, &rect, SDL_MapRGB(overlay->format, 255, 128, 0));
			}
            
            // Dibujar tambien un borde amarillo salido para resaltar aun mas
            //roundedRectangleRGBA(this->overlay, targetX, targetY, targetX + targetW, targetY + targetH, 5, 255, 255, 0, 255);
            //roundedRectangleRGBA(this->overlay, targetX + 1, targetY + 1, targetX + targetW - 1, targetY + targetH - 1, 5, 255, 255, 0, 255);
			//rectangleColor(overlay, targetX, targetY, targetX + targetW, targetY + targetH, SDL_MapRGB(overlay->format, 255, 255, 0));
			//rectangleColor(overlay, targetX + 1, targetY + 1, targetX + targetW - 1, targetY + targetH - 1, SDL_MapRGB(overlay->format, 255, 255, 0));
			rectangleRGBA(this->overlay, targetX, targetY, targetX + targetW, targetY + targetH, 255, 200, 0, 255);
            rectangleRGBA(this->overlay, targetX + 1, targetY + 1, targetX + targetW - 1, targetY + targetH - 1, 255, 200, 0, 255);
			

            // Volvemos a pintar el texto encima para que destaque bien sobre el nuevo fondo de seleccion
            if (font != nullptr && !keyb.caps[row][col].keyLabel.empty()) {
                SDL_Surface* textSurface = Fonts::renderUtf8Blended(font, keyb.caps[row][col].keyLabel.c_str(), keyb.textSelectedColor);
                if (textSurface != nullptr) {
                    SDL_Rect destRect;
                    destRect.x = targetX + (targetW - textSurface->w) / 2;
                    destRect.y = targetY + (targetH - textSurface->h) / 2;
                    SDL_BlitSurface(textSurface, nullptr, this->overlay, &destRect);
                    SDL_FreeSurface(textSurface);
                }
            }
        }
    }
}


void GameMenu::drawKeyboard(TTF_Font* font, t_keyboard& keyb){
    // 1. GENERACION DE LA CACHE (Solo se ejecuta la primera vez)
	const int kw = keyb.keyW;
	const int kh = keyb.keyH;

	int totalKeyboardW = (keyb.cols * (kw + keyb.spaceX));
    int totalKeyboardH = (keyb.rows * (kh + keyb.spaceY));
    if (keyb.keyboardSurface == nullptr) {
        SDL_Surface* rawSurface = SDL_CreateRGBSurface(SDL_SWSURFACE, totalKeyboardW, totalKeyboardH, 
                                                       this->overlay->format->BitsPerPixel,
                                                       this->overlay->format->Rmask, 
                                                       this->overlay->format->Gmask, 
                                                       this->overlay->format->Bmask, 
                                                       this->overlay->format->Amask);
        if (rawSurface == nullptr) return;

        // Nivel de opacidad de las teclas (0=transparente, 255=opaco). Como el
        // overlay se mezcla luego en D3D9 (XBOX_DrawOverlay con D3DBLEND_SRCALPHA),
        // este alpha controla cuanto se mezcla con el contenido detras (el juego).
        const Uint8 KEY_ALPHA = 180;
		Uint8 fillColorAlpha = KEY_ALPHA;

#ifndef _XBOX
		Uint32 colorkey = SDL_MapRGB(rawSurface->format, 255, 0, 255);
        SDL_FillRect(rawSurface, nullptr, colorkey);
        SDL_SetColorKey(rawSurface, SDL_SRCCOLORKEY, colorkey);
		fillColorAlpha = 0xFF;
#endif

        for (int row = 0; row < keyb.rows; row++){
            //int currentX = keyb.iniX;
			int currentX = 0;
            for (int col = 0; col < keyb.cols; col++){
                if (keyb.layoutWidth[row][col] == 0) continue;
                if (keyb.caps[row][col].h == 0) {
                    int defaultW = kw;
                    int actualW = (keyb.layoutWidth[row][col] * defaultW) + ((keyb.layoutWidth[row][col] - 1) * keyb.spaceX);
                    currentX += actualW + keyb.spaceX;
                    continue;
                }
                if (keyb.caps[row][col].keyLabel.empty()){
                    currentX += keyb.caps[row][col].w + keyb.spaceX;
                    continue;
                }    

                int keybPosX = currentX;
				int keybPosY = row * (kh + keyb.spaceY);

                // Dibujamos el fondo base de la tecla con alpha = KEY_ALPHA.
                // CLAVE: SDL_MapRGBA (no SDL_MapRGB) para que el byte alpha del
                // Uint32 empaquetado sea el que queremos, no 0. Si lo metieramos
                // con MapRGB o con un literal sin componente alta, los pixeles
                // saldrian con alpha=0 y el overlay no mostraria nada.
                SDL_Rect rect = { keybPosX, keybPosY, keyb.caps[row][col].w, keyb.caps[row][col].h };
                Uint32 keyBg = SDL_MapRGBA(rawSurface->format,
                                            Constant::colors[clRed].sdlColor.r,
                                            Constant::colors[clRed].sdlColor.g,
                                            Constant::colors[clRed].sdlColor.b,
                                            fillColorAlpha);

                SDL_FillRect(rawSurface, &rect, keyBg);

                if (font != nullptr && !keyb.caps[row][col].keyLabel.empty()) {
                    SDL_Surface* textSurface = Fonts::renderUtf8Blended(font, keyb.caps[row][col].keyLabel.c_str(), keyb.textColor);
                    if (textSurface != nullptr) {
                        SDL_Rect destRect;
                        destRect.x = keybPosX + (keyb.caps[row][col].w - textSurface->w) / 2;
                        destRect.y = keybPosY + (keyb.caps[row][col].h - textSurface->h) / 2;
                        destRect.w = textSurface->w;
                        destRect.h = textSurface->h;

                        SDL_BlitSurface(textSurface, nullptr, rawSurface, &destRect);
                        SDL_FreeSurface(textSurface);
                    }
                }
                currentX += keyb.caps[row][col].w + keyb.spaceX;
            }
        }
		
#ifdef _XBOX
        // NO convertimos con SDL_DisplayFormat (perderia el canal alpha) ni
        // con SDL_DisplayFormatAlpha (haria un SDL_SRCALPHA per-surface que
        // luego mezclaria INTERNAMENTE durante el blit en SDL, en lugar de
        // dejar el alpha-blending para D3D9). El rawSurface ya tiene el
        // formato exacto del overlay (clonamos sus mascaras al crearlo).
        //
        // Quitar SDL_SRCALPHA hace que el blit final (rawSurface ? overlay)
        // sea una COPIA literal de pixeles RGBA. El alpha de cada pixel
        // (KEY_ALPHA para fondos, 255 para texto, intermedio en bordes
        // antialiasados) llega intacto al overlay; XBOX_DrawOverlay despues
        // dibuja el quad con D3DBLEND_SRCALPHA / D3DBLEND_INVSRCALPHA y
        // mezcla cada pixel con el framebuffer del juego. Regla de SDL 1.2:
        //   RGBA?RGBA SIN SDL_SRCALPHA = copia pixeles tal cual.
        //   RGBA?RGBA CON SDL_SRCALPHA = SDL alpha-blendea internamente.
        SDL_SetAlpha(rawSurface, SDL_RLEACCEL, 0xFF);
        keyb.keyboardSurface = rawSurface;   // cacheamos directamente
#else 
		SDL_SetAlpha(rawSurface, SDL_SRCALPHA | SDL_RLEACCEL, KEY_ALPHA);
        keyb.keyboardSurface = SDL_DisplayFormatAlpha(rawSurface);
		SDL_FreeSurface(rawSurface);
#endif
    }

    // 2. VOLCADO ULTRA RAPIDO DEL TECLADO BASE
	SDL_Rect rect = { keyb.iniX, keyb.iniY, totalKeyboardW, totalKeyboardH};
    SDL_BlitSurface(keyb.keyboardSurface, nullptr, this->overlay, &rect);
	
	//Drawing the selected key
	drawSelectedKey(font, keyb, keyb.selectedRow, keyb.selectedCol);

	//Drawint a selected modifier if any
	for (int i=0; i < (int)keyb.pressedMods.size(); i++){
		drawSelectedKey(font, keyb, keyb.pressedMods[i].row, keyb.pressedMods[i].col);
	}
}


bool GameMenu::loadBgImage(){
	const dirutil dir;
	const ANIM_BACKGROUNDS bgType = static_cast<ANIM_BACKGROUNDS>(this->cfgLoader->configMain[cfg::animBG].valueInt);
	const std::string image = dir.getPathPrefix(getAssetsDir(cfgLoader->getCfgEmu()) + Constant::getFileSep() + BG_FILENAME) + ".jpg";

	bool found = dir.fileExists(image.c_str());
	if (bgType == BG_IMAGE && found){
		return this->bg_image.loadImage(image.c_str(), this->overlay->format);
	} else {
		this->bg_image.closeImage();
	}
	return false;
}

void GameMenu::checkDisplayOptions(){
	const auto &cfgEmu = getCfgLoader()->getCfgEmu();

	#ifdef SALVIA_GPU_VIDEO
	// Overriding the shader mode if the core has specified something greater than 0. 
	// 0 is actually -1 in the cfg file which represents "Auto"
	const int currentShader = cfgEmu->shaderMode > 0 ? cfgEmu->shaderMode - 1 : *this->current_shader;
	if (current_video_settings.filter != currentShader){
		XBOX_SelectEffect(currentShader);
		current_video_settings.filter = currentShader;
	}

	// Overriding the integer scale if the core has specified something greater than 0. 
	// 0 is actually -1 in the cfg file which represents "Auto"
	const bool currentIntegerScale = cfgEmu->integerScale > 0 ? (cfgEmu->integerScale - 1) == 1 : *this->current_integer_scale;
	if (current_video_settings.integer_scale != currentIntegerScale){
		SDL_XBOX_SetDisplayFullscreen(currentIntegerScale == false);
		current_video_settings.integer_scale = currentIntegerScale;
	}

	// Overriding the integer scale type if the core has specified something greater than 0. 
	// 0 is actually -1 in the cfg file which represents "Auto"
	const int currentIntegerScaleType = cfgEmu->scaleIntMode > 0 ? cfgEmu->scaleIntMode - 1 : *this->current_integer_scale_type;
	if (current_video_settings.integer_scale_type != currentIntegerScaleType){
		SDL_XBOX_SetDisplayOverflow(currentIntegerScaleType);
		current_video_settings.integer_scale_type = currentIntegerScaleType;
	}
	#endif

	// Overriding the aspect ratio if the core has specified something greater than 0. 
	// 0 is actually -1 in the cfg file which represents "Auto"
	float currentRatio = cfgEmu->aspectRatio > 0 ? aspectRatioValues[cfgEmu->aspectRatio - 1] : aspectRatioValues[*this->current_ratio];
	if (current_video_settings.ratio != currentRatio){
		LOG_DEBUG("SetDisplaySize: ratio=%.3f, tex=%dx%d", currentRatio, current_video_settings.sw, current_video_settings.sh);
		#ifdef SALVIA_GPU_VIDEO
		SDL_XBOX_SetDisplaySize(currentRatio);
		#endif
		current_video_settings.ratio = currentRatio;
	}
}
