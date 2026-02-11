#include "gamemenu.h"

#include <gfx/SDL_gfxPrimitives.h>
#include <gfx/gfx_utils.h>
#include <io/dirutil.h>
#include <beans/structures.h>
#include <so/launcher.h>
#include <libretro/libretro.h>

GameMenu::GameMenu(CfgLoader *cfgLoader){
    status = EMU_MENU;
	lastStatus = EMU_MENU;
	romLoaded = false;
	gameTicks.ticks = 0;
	video_page = NULL;
	dblBufferEnabled = true;
	this->cfgLoader = cfgLoader;
	this->initEngine(cfgLoader);
	int face_h = TTF_FontLineSkip(Fonts::getFont(Fonts::FONTSMALL));
	int pixelDato = 0;
	TTF_SizeText(Fonts::getFont(Fonts::FONTSMALL), "FPS: 888.8", &pixelDato, NULL);

	rectFps.x = this->screen->w - pixelDato - 3;
	rectFps.y = 0;
	rectFps.w = this->screen->w - rectFps.x;
	rectFps.h = face_h;
	bkgTextFps = SDL_MapRGB(this->screen->format, 0, 0, 0);
	uBkgColor = SDL_MapRGB(this->screen->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);

	if (!joystick->init_all_joysticks()){
		configButtonsJOY();
	}

	configMenus = new GestorMenus(screen->w, screen->h);
	configMenus->inicializar(cfgLoader, joystick);
	message.ticks = 0;
	// En la clase Config o GameMenu
	selectScalerMode(FULLSCREEN);
	this->current_scaler_mode = &getCfgLoader()->configMain[cfg::scaleMode].getIntRef();
	this->current_ratio = &getCfgLoader()->configMain[cfg::aspectRatio].getIntRef();
	this->current_sync = &getCfgLoader()->configMain[cfg::syncMode].getIntRef();
	this->current_force_fs = &getCfgLoader()->configMain[cfg::forceFS].getBoolRef();
	this->mustUpdateFps = &getCfgLoader()->configMain[cfg::showFps].getBoolRef();
	processConfigChanges();

	fpsSurface = NULL; 
	cpuSurface = NULL;
	bg_screenshot = NULL;
	lastFpsUpdate = 0;
	//initHqxFilter();
	setSavePath();
	scrapper = new Scrapper();
};

GameMenu::~GameMenu(){
	LOG_DEBUG("Deleting GameMenu...");
	delete configMenus;
	if (fpsSurface) SDL_FreeSurface(fpsSurface);
	if (cpuSurface) SDL_FreeSurface(cpuSurface);
	if (message.cache) SDL_FreeSurface(message.cache);
	if (bg_screenshot) SDL_FreeSurface(bg_screenshot);

	#ifndef _XBOX
		if (srf_32_convert.src32) SDL_FreeSurface(srf_32_convert.src32);
		if (srf_32_convert.dst32) SDL_FreeSurface(srf_32_convert.dst32);
		hqxClose();
	#endif
	delete scrapper;
}

std::string GameMenu::configButtonsJOY(){
    bool salir = false;
    string salida = "";
	Uint32 bkgText = SDL_MapRGB(this->screen->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);
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
        SDL_Event event;
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
//            pintarFillCircle(screen,
//                             imgW * imgButtonsRelScreen[i].x + imgX,
//                             imgh * imgButtonsRelScreen[i].y + imgY,
//                             40 * relacionAncho,
//                             SDL_MapRGB(screen->format, 255,0,0));

            //Dibujamos el texto de la accion
            drawTextCent(JoystickButtonsMSG[i], 0, 20, true, false, cBlanco);
            cachearObjeto(&obj);
        } else {
            cachearObjeto(&obj);
        }*/
		
		SDL_FillRect(this->screen, NULL, bkgText);
		Constant::drawTextCent(this->screen, Fonts::getFont(Fonts::FONTSMALL), FRONTEND_BTN_TXT[i], 0, 20, true, false, textColor, 0);
        SDL_Flip(this->screen);
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
							// CENTRO: Opcionalmente manejar el reposo aquí si es necesario
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

bool GameMenu::initDblBuffer(int w, int h){
#ifdef SW_DBL_BUFFER
    if (video_page != NULL){
		SDL_FreeSurface(video_page);
    }

	/* Create bitmap for page flipping */
	video_page = SDL_CreateRGBSurface(
        screen->flags,          // Mismos flags (SWSURFACE/HWSURFACE)
        screen->w,              // Mismo ancho
        screen->h,              // Mismo alto
        screen->format->BitsPerPixel, // Misma profundidad de color
        screen->format->Rmask,  // Máscara Roja
        screen->format->Gmask,  // Máscara Verde
        screen->format->Bmask,  // Máscara Azul
        screen->format->Amask   // Máscara Alfa
    );
	dblBufferEnabled = true;
#else 
	video_page = screen;
	dblBufferEnabled = false;
#endif
    return video_page != NULL;
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

/**
 * 
 */
void GameMenu::createMenuImages(ListMenu &listMenu){
    /** snap */
    Image imageSnap;
    const int snapW = screen->w / 2;
    const int snapH = listMenu.getH() / 2;
    const int snapOffset = screen->w / 10;
    //const int snapOffset = 5;
    menuImages.clear();
    menuTextAreas.clear();

    if (screen->w / 2 >= 320){
        imageSnap.setX(screen->w / 2 + snapOffset);
        imageSnap.setY(listMenu.getY());
        imageSnap.setW(snapW - snapOffset * 2);
        imageSnap.setH(snapH - snapOffset);
    } else {
        imageSnap.setX(screen->w / 2);
        imageSnap.setY(listMenu.getY());
        imageSnap.setW(snapW);
        imageSnap.setH(snapH);
    }
    imageSnap.vAlign = ALIGN_TOP;
    menuImages.insert(make_pair(SNAP, imageSnap));

    /** Box2d */
    Image imageBox2d;
    const int box2dH = listMenu.getH() / 4;
    const int box2dW = screen->w / 8;
    imageBox2d.setX(screen->w / 2);
    imageBox2d.setY(screen->h / 2 - box2dH);
    imageBox2d.setW(box2dW);
    imageBox2d.setH(box2dH);
    menuImages.insert(make_pair(BOX2D, imageBox2d));

    /** snaptit*/
    Image imageSnaptit;
    const int snapTitH = listMenu.getH() / 4;
    const int snapTitW = screen->w / 6;
    imageSnaptit.setX(screen->w - snapTitW);
    imageSnaptit.setY(screen->h / 2 - snapTitH);
    imageSnaptit.setW(snapTitW);
    imageSnaptit.setH(snapTitH);
    menuImages.insert(make_pair(SNAPTIT, imageSnaptit));

    Image imageSnapFs(0, 0, screen->w, screen->h);
    imageSnapFs.drawfaded = true;
    menuImages.insert(make_pair(SNAPFS, imageSnapFs));
    
    const int sectionGap = 0;
    const int textAreaY = listMenu.getH() / 2 + listMenu.getY() + sectionGap;
    TextArea textarea(screen->w / 2, textAreaY, screen->w / 2, screen->h - textAreaY);
    textarea.marginX = (int)floor((double)screen->w / 100);
    menuTextAreas.insert(make_pair(SYNOPSIS, textarea));
}

/**
 * 
 */
void GameMenu::refreshScreen(ListMenu &listMenu){
	ConfigEmu emu = *cfgLoader->getCfgEmu();
    //Drawing the emulator name
    TTF_Font *fontBig = Fonts::getFont(Fonts::FONTBIG);
    TTF_Font *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
    const int sepVertX = listMenu.getW();
    const int halfWidth = screen->w / 2;
	int face_h_big = TTF_FontLineSkip(fontBig);
	int face_h_small = TTF_FontLineSkip(fontsmall);
	bool debug = true;

    //Drawing the rest of list and images
    if (listMenu.getNumGames() > (std::size_t)listMenu.curPos){
        auto game = listMenu.listGames.at(listMenu.curPos).get();
        
        if (!game->shortFileName.empty()){
            if (listMenu.layout == LAYBOXES) {
				Constant::drawTextCentTransparent(video_page, fontBig, emu.name.c_str(), 0, face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
					true, false, textColor, 0);
                
				fastline(this->video_page, listMenu.marginX, listMenu.marginY - 1 , screen->w - listMenu.marginX, listMenu.marginY - 1, menuBars);
                fastline(this->video_page, sepVertX, listMenu.marginY , sepVertX, listMenu.getH() + listMenu.marginY - 1, menuBars);
                listMenu.draw(this->video_page);

                //Drawing a transparent rectangle
                //if (screen->w >= 640){
                    //static const int transBGText = SDL_MapRGBA(this->video_page->format, 255, 255, 255, 20);
					//SDL_Rect rec = {halfWidth + 1, listMenu.marginY, screen->w - (halfWidth + 2), screen->h - listMenu.marginY - 1};
					//DrawRectAlpha(this->video_page, rec, white, 160);
                //}

                //Draw and update the screen because the loading of images can take a long time
                if (listMenu.keyUp){
                    string assetsDir = getPathPrefix(emu.assets) + string(Constant::tempFileSep);
                    //Drawing the rom's synopsis text
                    menuTextAreas[SYNOPSIS].loadTextFileFromGame(assetsDir + "synopsis" + string(Constant::tempFileSep), *game, ".txt");
                    menuTextAreas[SYNOPSIS].resetTicks(this->gameTicks);
                    menuTextAreas[SYNOPSIS].draw(this->video_page, this->gameTicks);

                    //Snapshot picture
                    menuImages[SNAP].loadImageFromGame(assetsDir + "snap" + string(Constant::tempFileSep), *game, ".png");
                    menuImages[SNAP].printImage(this->video_page);
                    blit(this->video_page, screen, 0, 0, 0, 0, this->video_page->w, this->video_page->h);

                    if (screen->w < 640){
                        //If it's so small, only show the snapshot
                        return;
                    }

                    //Box picture
                    menuImages[BOX2D].loadImageFromGame(assetsDir + "box2d" + string(Constant::tempFileSep), *game, ".png");
                    menuImages[BOX2D].printImage(this->video_page);
                    blit(this->video_page, screen, 0, 0, 0, 0, this->video_page->w, this->video_page->h);

                    //Title picture
                    menuImages[SNAPTIT].loadImageFromGame(assetsDir + "snaptit" + string(Constant::tempFileSep), *game, ".png");
                    menuImages[SNAPTIT].printImage(this->video_page);
                    blit(this->video_page, screen, 0, 0, 0, 0, this->video_page->w, this->video_page->h);
                } else {
                    menuImages[SNAP].printImage(this->video_page);
                    menuImages[BOX2D].printImage(this->video_page);
                    menuImages[SNAPTIT].printImage(this->video_page);
                    menuTextAreas[SYNOPSIS].draw(this->video_page, this->gameTicks);
                }

            } else if (listMenu.layout == LAYSIMPLE) {
                if (listMenu.keyUp){
                    //Snapshot picture
                    menuImages[SNAPFS].loadImageFromGame(getPathPrefix(emu.assets) + string(Constant::tempFileSep)
                        + "snap" + string(Constant::tempFileSep), *game, ".png");
                }
                menuImages[SNAPFS].printImage(this->video_page);
                //Draw the menu element after the image
                Constant::drawTextCent(video_page, fontBig, emu.name.c_str(), 
					halfWidth, face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
					true, false, textColor, 0);

                fastline(this->video_page, listMenu.marginX, listMenu.marginY - 1, listMenu.getW(), listMenu.marginY - 1, textColor);
                listMenu.draw(this->video_page);

            } else if (listMenu.layout == LAYTEXT) {

				Constant::drawTextCent(video_page, fontBig, emu.name.c_str(), 
					halfWidth, face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
					true, false, textColor, 0);

                fastline(this->video_page, listMenu.marginX, listMenu.marginY - 1, listMenu.getW(), listMenu.marginY - 1, textColor);
                listMenu.draw(this->video_page);
            }
        }
	} else if (listMenu.getNumGames() == 0 && emu.generalConfig){
		configMenus->draw(video_page);
    } else if (listMenu.getNumGames() == 0){
		//Constant::drawTextCent(video_page, fontBig, emu.name.c_str(), 
		//			cfgLoader->getWidth(), face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
		//			true, false, textColor, 0);

		Constant::drawTextCentTransparent(video_page, fontBig, emu.name.c_str(), 0, face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
					true, false, textColor, 0);

        fastline(this->video_page, listMenu.marginX, listMenu.marginY - 1, screen->w - listMenu.marginX, listMenu.marginY - 1, textColor);
		Constant::drawTextCent(video_page, fontsmall, "No roms found", 0, 0, true, true, textColor, 0);

		// Para renderizar, usas el puntero de la clase
        Menu* m = configMenus->obtenerMenuActual();
        // Dibujar m->titulo y m->opciones[i]->titulo...

    } else {
		Constant::drawTextCent(video_page, fontsmall, "The configuration is not valid", 0, 0, true, true, textColor, 0);
		Constant::drawTextCent(video_page, fontsmall, "Press TAB to select the next entry or", 0, face_h_small + 3, true, true, textColor, 0);
		Constant::drawTextCent(video_page, fontsmall, "Press ESC to exit", 0, (face_h_small + 3) * 2, true, true, textColor, 0);
    }
}

/**
*
*/
void GameMenu::blit(SDL_Surface * src, SDL_Surface * dst, int x1, int y1, int w1, int h1, int w2, int h2){
	if (this->dblBufferEnabled){
		SDL_Rect srcRect = {x1, y1, w1, h1};
		SDL_Rect dstRect = {0, 0, w2, h2};
		SDL_BlitSurface(this->video_page, &srcRect, screen, &dstRect);
	} 
}

/**
 * 
 */
void GameMenu::showMessage(string msg){
    int startGray = 240;
    static const int bkg = SDL_MapRGB(this->video_page->format, startGray, startGray, startGray);
    TTF_Font *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
	int face_h_small = TTF_FontLineSkip(fontsmall);
    
    int rw = Fonts::getSize(Fonts::FONTSMALL, msg.c_str()) + 5; 
    //int rh = this->video_page->h / 3;
    int rh = face_h_small * 2;
    int rx = (this->video_page->w - rw) / 2;
    int ry = (this->video_page->h - rh) / 2 + face_h_small / 2;

	SDL_Rect rect = {rx, ry, rw, rh};
    //drawing_mode(DRAW_MODE_TRANS, this->video_page, rx, ry);
    SDL_FillRect(this->video_page, &rect, bkg);
    
    const int step = 40;
    for (int i=1; i < 5; i++){
        int fadingBkg = SDL_MapRGB(this->video_page->format, startGray - i*step, startGray - i*step, startGray - i*step);
		//drawing_mode(DRAW_MODE_TRANS, this->video_page, rx, ry);
		rectangleColor(this->video_page, rx - i, ry - i, rx + rw + i, ry + rh + i, fadingBkg);
    }

    //drawing_mode(DRAW_MODE_SOLID, this->video_page, rx, ry);
	Constant::drawTextCent(video_page, fontsmall, msg.c_str(), 
		this->video_page->w / 2, this->video_page->h / 2, true, true, black, -1);

	SDL_BlitSurface(this->video_page, NULL, screen, NULL);
}

/**
 * 
 */
void GameMenu::loadEmuCfg(ListMenu &menuData){
    TTF_Font *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
	int face_h_small = TTF_FontLineSkip(fontsmall);
	static const int cblack = SDL_MapRGB(this->video_page->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);

    if (cfgLoader->emulators.size() == 0){
        SDL_FillRect(screen, NULL, cblack);
        string msg = "There are no emulators configured. Exiting..."; 
		Constant::drawTextCent(screen, fontsmall, msg.c_str(), 0, 0, true, true,  white, -1);
		Constant::drawTextCent(screen, fontsmall, "Press a key to continue", 0, face_h_small + 3, true, true, white, -1);
		SDL_Flip(screen);
        SDL_Delay(3000);
		return;
    }

	if (cfgLoader->emulators.size() <= (std::size_t)cfgLoader->emuCfgPos){
        cfgLoader->emuCfgPos = 0;
    } 

    dirutil dir;
	ConfigEmu *emu = cfgLoader->getCfgEmu();
    string mapfilepath = Constant::getAppDir() //+ string(Constant::tempFileSep) + "gmenu" 
            + string(Constant::tempFileSep) + "config" + string(Constant::tempFileSep) + emu->map_file;
    
    if (emu->use_rom_file && !emu->map_file.empty() && dir.fileExists(mapfilepath.c_str())){
        menuData.mapFileToList(mapfilepath);
    } else {
        mapfilepath = getPathPrefix(emu->rom_directory);
        vector<unique_ptr<FileProps>> files;
		
		string extFilter = " " + emu->rom_extension;
        extFilter = Constant::replaceAll(extFilter, " ", ".");

		if (isDebug()){
            SDL_FillRect(screen, NULL, cblack);
            string msg = "searching " + mapfilepath; 
			Constant::drawTextCent(screen, fontsmall, msg.c_str(), screen->w / 2, screen->h / 2, true, true,  white, -1);
        }

        dir.listarFilesSuperFast(mapfilepath.c_str(), files, extFilter, true, false);

		ConfigEmu emu = *cfgLoader->getCfgEmu();
        string mapfilepath = getPathPrefix(emu.rom_directory);

        if (isDebug()){
            SDL_FillRect(screen, NULL, cblack);
            string msg = "roms found: " + Constant::TipoToStr(files.size()); 
            string msg2 = "In dir " + mapfilepath;
			Constant::drawTextCent(screen, fontsmall, msg.c_str(), 0, 0, true, true,  white, -1);
            Constant::drawTextCent(screen, fontsmall, msg2.c_str(), 0, face_h_small + 3, true, true,  white, -1);
			Constant::drawTextCent(screen, fontsmall, "Press a key to continue", 0, (face_h_small + 3) * 2, true, true,  white, -1);
            SDL_Delay(3000);
        }

        menuData.filesToList(files, emu);
        files.clear();
    }
}

/**
 * 
 */
string GameMenu::getPathPrefix(string filepath){
	ConfigEmu emu = *cfgLoader->getCfgEmu();
	string finalpath;
	cfgLoader->configMain[cfg::path_prefix].getPropValue(finalpath);

	if (finalpath.at(finalpath.length()-1) != Constant::getFileSep()[0] && filepath.at(0) != Constant::getFileSep()[0]){
		finalpath += Constant::getFileSep();
	}
    finalpath += filepath;

    string drivestr = string(":") + string(Constant::tempFileSep);
    //Checking if the path to the roms is absolute
    if (!filepath.empty() && (filepath.at(0) == Constant::getFileSep()[0] || filepath.find(drivestr) != string::npos)){
        finalpath = filepath;
    }
    #if defined(WIN) || defined(DOS)
        finalpath = Constant::replaceAll(finalpath, "/", "\\");
    #endif
    return finalpath;
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
vector<string> GameMenu::launchProgram(ListMenu &menuData){
    //Launcher launcher;
    dirutil dir;
    vector<string> commands;

    if (cfgLoader->emulators.size() <= (std::size_t)cfgLoader->emuCfgPos)
        return commands;

	ConfigEmu emu = *cfgLoader->getCfgEmu();
	string pathPrefix = getPathPrefix(emu.directory);
	
	if (pathPrefix.at(pathPrefix.length()-1) != Constant::tempFileSep[0]){
		pathPrefix += string(Constant::tempFileSep);
	}
	pathPrefix += emu.executable;

    commands.emplace_back(pathPrefix);
    
    if (emu.options_before_rom){
        vector<string> v = Constant::splitChar(emu.global_options, ' ');
        //for (auto s : v){
		for (unsigned int i=0; i < v.size(); i++){
			std::string s = v.at(i);
            commands.emplace_back(s);
        }
    }

    if (menuData.listGames.size() <= (std::size_t)menuData.curPos)
        return commands;

    auto game = menuData.listGames.at(menuData.curPos).get();
    
    //Ignoring the fields if a rom file is used
    if (emu.use_rom_file){
        commands.emplace_back(encloseWithCharIfSpaces(game->shortFileName, "\"")); 
    } else {
        string romdir = emu.use_rom_directory ? getPathPrefix(emu.rom_directory) + string(Constant::tempFileSep) : "";
        string romFile = game->longFileName;

        #ifdef DOS
            //Maybe the long filename support is activated on msdos,
            //Otherwise just pick up the shortfilename
            string fileabslongname = encloseWithCharIfSpaces(romdir + game->longFileName, "\"");
            string msg = "romdir: " + romdir;
            string msg1;
            string msg2;

            if (!dir.fileExists( fileabslongname.c_str()  )){
                romFile = game->shortFileName;
                msg1 = "file " + game->longFileName + "doesn't exists.";
                msg2 = "launching " + romFile;
            } else {
                msg1 = "launching " + romFile;
            }

            if (cfgLoader->configMain.debug){
                clear(screen);
                TTF_Font *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
                Constant::drawTextCentre(screen, fontsmall, msg.c_str(), screen->w / 2, screen->h / 2, textColor, -1);
                Constant::drawTextCentre(screen, fontsmall, msg1.c_str(), screen->w / 2, screen->h / 2 + (fontsmall->face_h + 3) * 2, textColor, -1);
                Constant::drawTextCentre(screen, fontsmall, msg2.c_str(), screen->w / 2, screen->h / 2 + (fontsmall->face_h + 3) * 3, textColor, -1);
                Constant::drawTextCentre(screen, fontsmall, "Press a key to continue", screen->w / 2, screen->h / 2 + (fontsmall->face_h + 3) * 4, textColor, -1);
                readkey();
            }
        #endif

        string rom = emu.use_extension ? romFile : dir.getFileNameNoExt(romFile);

		#ifdef LIBRETRO
			std::string execActual = Constant::getAppExecutable();
			if (emu.executable.find(execActual) != string::npos){
				//Tenemos que lanzar la rom en el propio ejecutable porque el soporte es correcto para esta rom
				commands.emplace_back(romdir + rom);
				return commands;
			} else {
				#ifdef _XBOX
					commands.emplace_back(romdir + rom);
				#else
					commands.emplace_back(encloseWithCharIfSpaces(romdir + rom, "\"")); 
				#endif
			}
		#else
			commands.emplace_back(encloseWithCharIfSpaces(romdir + rom, "\"")); 
		#endif	
    }

    //string romToLaunch = romdir + rom;
    //if (!dir.fileExists(romToLaunch.c_str())){
    //    clear_to_color(screen, backgroundColor);
    //    string msg = "roms doesn't exist: " + romToLaunch; 
    //    textout_centre_ex(screen, font, msg.c_str(), screen->w / 2, screen->h / 2, textColor, -1);
    //    textout_centre_ex(screen, font, "Press a key to continue", screen->w / 2, screen->h / 2 + (font->height + 3) * 2, textColor, -1);
    //    readkey();
    //    return; // or handle the error as needed
    //}

    if (!emu.options_before_rom){
        commands.emplace_back(emu.global_options);
        //vector<string> v = Constant::splitChar(emu.global_options, ' ');
        //for (auto s : v){
        //    commands.emplace_back(s);
        //}
    }

    saveGameMenuPos(menuData);

    //For some reason, with Alsa, when launching retroarch, the sound must be deactivated. Otherwise, it freezes
    bool resetAudio = false;
    #ifdef UNIX
        resetAudio = true;
    #endif

	LOG_DEBUG("Launching %s\n", commands.at(0).c_str());

	#ifdef LIBRETRO
		Launcher launcher;
		this->running = !launcher.launch(commands, isDebug());
	#endif

    /**TODO: IMPLEMENT
	if (cfgLoader->configMain.alsaReset && resetAudio && Constant::getExecMethod() != launch_batch ){
        remove_sound();
    }

    //if we are in fullscreen, switch to windowed, because the launched app maybe not 
    //be showed in the first plane
    bool isFullscreen = !is_windowed_mode();
    #ifdef DOS
        //In MSDOS we don't need to do the fullscreen switch
        isFullscreen = false;
    #endif

    if (isFullscreen)
        swithScreenFullWindow(*this->cfgLoader);

    launcher.launch(commands, cfgLoader->configMain.debug);

    //if we were in fullscreen, switch back to fullscreen
    if (isFullscreen)
        swithScreenFullWindow(*this->cfgLoader);
    
    //Try to reactivate sound, although it's pointless in my tests. I couldn't make it to work with Alsa.
    //Install pipewire instead
    if (cfgLoader->configMain.alsaReset && resetAudio && Constant::getExecMethod() != launch_batch ){
        this->initSound();
    }
	*/

	return commands;
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
        cerr << "Error openning file: " << filepath << endl;
        return 1;
    }

    struct ListStatus input1 = { cfgLoader->emuCfgPos, menuData.iniPos, menuData.endPos, 
        menuData.curPos, menuData.maxLines, menuData.layout, menuData.animateBkg};

    int flag = 0;
    flag = fwrite(&input1, sizeof(struct ListStatus), 1, outfile);

    if (flag) {
        cout << "Contents of the structure written successfully" << endl;
    } else {
        cerr << "Error Writing to File: " << filepath << endl;
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
        cout << "emupos: " << read_struct.emuLoaded << "; inipos: " << read_struct.iniPos
            << "; endpos: " << read_struct.endPos << "; curpos: " << read_struct.curPos 
            << "; maxlines: " << read_struct.maxLines
            << "; layout: " << read_struct.layout
            << "; animateBkg: " << read_struct.animateBkg << endl;
        //Setting the emulator selected        
        cfgLoader->emuCfgPos = read_struct.emuLoaded;
    } else {
        ret = 1;
    }

    fclose(infile);
    return ret;
}

void GameMenu::updateFps(){
    if (*this->mustUpdateFps) {
        uint32_t currentTick = SDL_GetTicks();
		const bool shouldUpdateSurface = currentTick - lastFpsUpdate > 500 || fpsSurface == NULL;

        // 1. Calculamos la media (esto es rápido, solo matemáticas)
		//this->sync->update_fps_counter(shouldUpdateSurface);
		this->sync->update_fps_counter(shouldUpdateSurface);

        // 2. ¿Ha pasado tiempo suficiente para actualizar el NÚMERO? (ej. cada 500ms)
        if (shouldUpdateSurface) {
            // Liberamos la superficie anterior
            if (fpsSurface) SDL_FreeSurface(fpsSurface);
			if (cpuSurface) SDL_FreeSurface(cpuSurface);
			
            // Creamos la nueva superficie con el texto actualizado
            // Asumimos que esta función devuelve una SDL_Surface* nueva
			fpsSurface = TTF_RenderText(Fonts::getFont(Fonts::FONTSMALL), this->sync->fpsText, white, black);
			cpuSurface = TTF_RenderText(Fonts::getFont(Fonts::FONTSMALL), this->sync->cpuText, white, black);
            lastFpsUpdate = currentTick;
        }

        // 3. DIBUJO (Esto se hace EN CADA FRAME y es muy rápido)
        if (fpsSurface && cpuSurface) {
            // Borramos el fondo del OSD
            SDL_FillRect(this->screen, &rectFps, bkgTextFps);
            SDL_BlitSurface(fpsSurface, NULL, this->screen, &rectFps);
			SDL_Rect rectCpu = {rectFps.x, rectFps.y + fpsSurface->h, this->screen->w - rectFps.x, fpsSurface->h};
			SDL_FillRect(this->screen, &rectCpu, bkgTextFps);
			SDL_BlitSurface(cpuSurface, NULL, this->screen, &rectCpu);
        }
    }
}

void GameMenu::processFrontendEvents(HOTKEYS_LIST hotkey){
	processHotkeys(hotkey);
}

void GameMenu::processFrontendEventsAfter(){
	// Actualizamos el contador de media de fps
	updateFps();
	//Mostramos mensajes
	processMessages();
	//Actualizamos para detectar cuando soltamos un boton
	processKeyUp();
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

	static int actualFilter = 0;
	int modeOk = true;
	int startingMode = *this->current_scaler_mode;

	struct retro_system_av_info av_info;
	retro_get_system_av_info(&av_info);
	const unsigned ancho_base = av_info.geometry.base_width;
	const unsigned alto_base = av_info.geometry.base_height;

	switch (hotkey){
		case HK_SCALE:
			do {
				*this->current_scaler_mode = ((*this->current_scaler_mode + 1) % TOTAL_VIDEO_SCALE);

				const int dw = this->video_page->w;
				const int dh = this->video_page->h;
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
				} else if (*this->current_force_fs && (*current_scaler_mode == SCALE4X || *current_scaler_mode == SCALE3X 
							|| *current_scaler_mode == SCALE2X || *current_scaler_mode == SCALE1X)) {
					//Si queremos pantalla completa, no tiene sentido pasemos por un scalenx.
					modeOk = false;
				} else {
					modeOk = true;
				}
			} while(!modeOk && *current_scaler_mode != startingMode);

			LOG_INFO("Escaler %d - %s\n", *current_scaler_mode, videoScaleStrings[*current_scaler_mode]);

			selectScalerMode(*current_scaler_mode);
			SDL_FillRect(this->video_page, NULL, this->uBkgColor);
			showSystemMessage(videoScaleStrings[*current_scaler_mode], 3000);
			break;

		case HK_RATIO:
			*this->current_ratio = (*this->current_ratio + 1) % TOTAL_VIDEO_RATIO;
			SDL_FillRect(this->video_page, NULL, this->uBkgColor);
			showSystemMessage(aspectRatioStrings[*this->current_ratio], 3000);
			break;

		case HK_SHADER:
			#ifdef _XBOX
				//Some tinkering with shaders
				XBOX_SelectEffect((++actualFilter) % 3);
			#endif
			showSystemMessage("changing filter", 3000);
			break;
		case HK_EXIT_GAME:
			setEmuStatus(EMU_MENU);
			break;
		case HK_VIEW_MENU:
			setEmuStatus(getEmuStatus() == EMU_MENU_OVERLAY ? getLastStatus() : EMU_MENU_OVERLAY);
			if (bg_screenshot){
				SDL_FreeSurface(bg_screenshot);
				bg_screenshot = NULL;
			}

			if (getEmuStatus() == EMU_MENU_OVERLAY && screen){
				bg_screenshot = clonarPantalla(screen, 180);
				configMenus->poblarPartidasGuardadas(getCfgLoader(), getRomPaths()->rompath);
			}
			break;
	}
	//joystick->resetButtonsCore();
}

/**
*
*/
SDL_Surface* GameMenu::clonarPantalla(SDL_Surface* src, int transparency) {
    if (!src) return NULL;

    // Bloqueamos la superficie si es necesario (común en buffers de video directos)
    if (SDL_MUSTLOCK(src)) {
        if (SDL_LockSurface(src) < 0) return NULL;
    }

	
    // Creamos la copia con el mismo formato exacto
    SDL_Surface* copia = SDL_ConvertSurface(src, src->format, src->flags);

	if (transparency > 0){
		// 1. Crear una superficie temporal del mismo tamaño que la original
		// Usamos las mismas máscaras de bits para compatibilidad
		SDL_Surface* overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, 
										copia->w, 
										copia->h, 
										copia->format->BitsPerPixel,
										copia->format->Rmask, 
										copia->format->Gmask, 
										copia->format->Bmask, 
										copia->format->Amask);

		if (overlay) {
			// 2. Pintar la superficie de negro
			SDL_FillRect(overlay, NULL, SDL_MapRGB(overlay->format, 0, 0, 0));
			// 3. Configurar el nivel de transparencia (0 = invisible, 255 = opaco)
			// SDL_SRCALPHA activa el blending por pixel o por superficie
			SDL_SetAlpha(overlay, SDL_SRCALPHA, transparency);
			// 4. Dibujar el rectángulo negro sobre la superficie original
			SDL_BlitSurface(overlay, NULL, copia, NULL);
			// 5. Liberar la memoria de la superficie temporal
			SDL_FreeSurface(overlay);
		}
	}

    if (SDL_MUSTLOCK(src)) {
        SDL_UnlockSurface(src);
    }

    return copia;
}

void GameMenu::selectScalerMode(int mode){
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
					//current_scaler = scale_software_fixed_point_ppc;		//110fps
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
			//current_scaler = scale_hqnx_alt;
			#ifdef _XBOX
			current_scaler = scale_hq2x_xbox;
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
}

void GameMenu::showSystemMessage(std::string text, uint32_t duration) {
	if (message.cache) SDL_FreeSurface(message.cache);
    
    message.content = text;
    message.timeout = duration;
    message.ticks = SDL_GetTicks();
	
	std::string newText = Fonts::recortarAlTamanyo(text, this->screen->w);
	// Renderizamos el mensaje una sola vez
    message.cache = TTF_RenderText_Blended(Fonts::getFont(Fonts::FONTBIG), newText.c_str(), white);
    
    if (message.cache) {
        int face_h = TTF_FontLineSkip(Fonts::getFont(Fonts::FONTBIG));
        message.rect.x = 0;
        message.rect.y = this->video_page->h - face_h - 4;
        message.rect.w = message.cache->w + 2;
        message.rect.h = face_h + 4;
    }
}

void GameMenu::processMessages(){
	if (message.ticks == 0 || !message.cache) return;

    const Uint32 now = SDL_GetTicks();

    if (now - message.ticks < message.timeout) {
        // 1. Dibujar el fondo del mensaje
        SDL_FillRect(this->video_page, &message.rect, this->uBkgColor);
        
        // 2. Copiar el texto ya renderizado (Operación ultra rápida)
        SDL_Rect dstText = { message.rect.x + 1, message.rect.y + 2, 0, 0 };
        SDL_BlitSurface(message.cache, NULL, this->video_page, &dstText);
    } else {
        // El mensaje ha expirado
        SDL_FreeSurface(message.cache);
        message.cache = NULL;
        message.ticks = 0;
		// 1. Dibujar el fondo del mensaje
        SDL_FillRect(this->video_page, &message.rect, this->uBkgColor);
    }
}

void GameMenu::processConfigChanges(){
	selectScalerMode(*this->current_scaler_mode);
}

void GameMenu::setRomPaths(std::string rp){
	dirutil dir;
	getRomPaths()->rompath = rp;

	std::string statesDir = getCfgLoader()->configMain[cfg::libretro_state].valueStr + Constant::getFileSep() +
		getCfgLoader()->configMain[cfg::libretro_core].valueStr;
			
	if (!dir.dirExists(statesDir.c_str())){
		dir.createDirRecursive(statesDir.c_str());
	}

	getRomPaths()->savestate = statesDir + Constant::getFileSep() + 
		dir.getFileNameNoExt(rp) + STATE_EXT;

	std::string sramDir = getCfgLoader()->configMain[cfg::libretro_save].valueStr + Constant::getFileSep() +
		getCfgLoader()->configMain[cfg::libretro_core].valueStr;
			
	if (!dir.dirExists(sramDir.c_str())){
		dir.createDirRecursive(sramDir.c_str());
	}

	getRomPaths()->sram = sramDir + Constant::getFileSep() + 
		dir.getFileNameNoExt(rp) + ".srm";
}

void GameMenu::setSavePath(){
	dirutil dir;

	std::string savesDir = getCfgLoader()->configMain[cfg::libretro_save].valueStr + Constant::getFileSep() +
		getCfgLoader()->configMain[cfg::libretro_core].valueStr;
			
	if (!dir.dirExists(savesDir.c_str())){
		dir.createDirRecursive(savesDir.c_str());
	}

	getRomPaths()->saves = savesDir;
}

void GameMenu::startScrapping(){
	LOG_DEBUG("Starting the scrap process");

	ScrapperConfig config;

	switch (this->configMenus->getScrapGamesSelection()){
		case SCRAP_NO_METADATA:
			config.downloadNoMetadata = true;
			break;
		case SCRAP_NO_SCREENSHOT:
			config.downloadNoSS = true;
			break;
		case SCRAP_NO_BOX:
			config.downloadNoBox = true;
			break;
		case SCRAP_NO_TITLE:
			config.downloadNoTitle = true;
			break;
	}
	int regIndex;
	int langIndex;
	cfgLoader->configMain[cfg::scrapRegion].getPropValue(regIndex);
	cfgLoader->configMain[cfg::scrapLang].getPropValue(langIndex);

	config.lenguaPreferida = this->configMenus->getLangCode(langIndex);
	config.regionPreferida = this->configMenus->getRegionCode(regIndex);
	LOG_DEBUG("Seleccionando lengua %s y region %s", config.lenguaPreferida.c_str(), config.regionPreferida.c_str());

	for (int i=0; i < cfgLoader->emulators.size() - 1; i++){
		if (this->configMenus->scrapSelection[i].selected){
			int idxEmu = this->configMenus->scrapSelection[i].index;
			if (idxEmu >= 0 && idxEmu < cfgLoader->emulators.size() - 1){
				LOG_DEBUG("Scrapping %s", this->configMenus->scrapSelection[idxEmu].name.c_str());
				scrapper->scrapSystem(cfgLoader->emulators[idxEmu].get()->config, config);
			}
		}
	}
}