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

/* Definida en salvia.cpp — devuelve los descriptores de memoria que el core
 * envio via RETRO_ENVIRONMENT_SET_MEMORY_MAPS (ej. HRAM en Game Boy). */
extern const struct retro_memory_descriptor* get_core_memory_descriptors(unsigned* out_count);

vector<SDL_Surface *> Icons::icons;
vector<SDL_Surface *> Icons::icons_carts;

GameMenu::GameMenu(CfgLoader *cfgLoader){
    status = EMU_MENU;
	lastStatus = EMU_MENU;
	romLoaded = false;
	gameTicks.ticks = 0;
	this->cfgLoader = cfgLoader;
	this->initEngine(cfgLoader);

	int face_h_big = TTF_FontLineSkip(Fonts::getFont(Fonts::FONTSMALL));
	std::string initMsg = "Loading " + Constant::getAppExecutable() + "...";
	Constant::drawTextCentTransparent(overlay, Fonts::getFont(Fonts::FONTBIG), initMsg.c_str(), 0, -face_h_big / 2, true, true, textColor, 0);
	SDL_Flip(gameScreen);

	int face_h = TTF_FontLineSkip(Fonts::getFont(Fonts::FONTSMALL));
	int pixelDato = 0;
	TTF_SizeText(Fonts::getFont(Fonts::FONTSMALL), "FPS: 888.8", &pixelDato, NULL);

	rectFps.x = this->overlay->w - pixelDato - 3;
	rectFps.y = 2*face_h;
	rectFps.w = this->overlay->w - rectFps.x;
	rectFps.h = face_h;
	bkgTextFps = SDL_MapRGB(this->overlay->format, 0, 0, 0);
	uBkgColor = SDL_MapRGB(this->overlay->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);

	if (!joystick->init_all_joysticks()){
		configButtonsJOY();
	}

	for (int i=0; i < clTotalColors; i++){
		colors[i].color = SDL_MapRGBA(this->overlay->format, colors[i].sdlColor.r, colors[i].sdlColor.g, colors[i].sdlColor.b, 0xFF);
	}

	configMenus = new GestorMenus(overlay->w, overlay->h);
	configMenus->inicializar(cfgLoader, joystick);
	// En la clase Config o GameMenu
	selectScalerMode(FULLSCREEN);

	this->current_scaler_mode = &getCfgLoader()->configMain[cfg::scaleMode].getIntRef();
	this->current_ratio = &getCfgLoader()->configMain[cfg::aspectRatio].getIntRef();
	this->current_sync = &getCfgLoader()->configMain[cfg::syncMode].getIntRef();
	this->current_force_fs = &getCfgLoader()->configMain[cfg::forceFS].getBoolRef();
	this->current_shader = &getCfgLoader()->configMain[cfg::shaderMode].getIntRef();
	this->mustUpdateFps = &getCfgLoader()->configMain[cfg::showFps].getBoolRef();
	processConfigChanges();

	fpsSurface = NULL; 
	cpuSurface = NULL;
	memSurface = NULL;
	bg_screenshot = NULL;
	lastFpsUpdate = 0;
	lastMemUpdate = 0;
	cargarSystemAchievementTranslation(Constant::getAppDir() + ROUTE_ACHIEVEMENT_TRANSLATIONS);
	initAchievements();
	Icons::loadIcons();
	Launcher::initDrives();
};

GameMenu::~GameMenu(){
	LOG_DEBUG("Deleting GameMenu...");
	delete configMenus;
	Icons::freeIcons();

	if (fpsSurface) SDL_FreeSurface(fpsSurface);
	if (cpuSurface) SDL_FreeSurface(cpuSurface);
	if (memSurface) SDL_FreeSurface(memSurface);
	for (unsigned int i=0; i < messages.size(); i++) {
		SDL_FreeSurface(messages[i].cache);
	}
		
	if (bg_screenshot) SDL_FreeSurface(bg_screenshot);

	#ifndef _XBOX
		if (srf_32_convert.src32) SDL_FreeSurface(srf_32_convert.src32);
		if (srf_32_convert.dst32) SDL_FreeSurface(srf_32_convert.dst32);
		hqxClose();
	#endif

	Achievements::instance()->shutdown();
}

void GameMenu::initAchievements(){
	Achievements::instance()->setHardcoreMode(getCfgLoader()->configMain[cfg::hardcoreRA].valueBool);
	const std::string user = getCfgLoader()->configMain[cfg::raUser].valueStr;
	const std::string pass = getCfgLoader()->configMain[cfg::raPass].valueStr;
	Achievements::instance()->login(user.c_str(), pass.c_str());
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
            
        // 1. Buscamos el inicio de la sección
        if (line == "[SALVIA_TO_ACHIEVEMENTS]") {
            seccionEncontrada = true;
            continue; // Pasamos a la siguiente línea
        }

        // 2. Si ya estamos en la sección correcta, procesamos los datos
        if (seccionEncontrada) {
            // Si encontramos otra sección (empieza por [), dejamos de leer
            if (!line.empty() && line[0] == '[') {
                break; 
            }

            // Ignorar comentarios o líneas vacías
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
    // 3. Obtener punteros de memoria (Asegúrate de que el Core ya está cargado)
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
		if (descs && desc_count > 0) {
			Achievements::instance()->set_core_descriptors(descs, desc_count);
		}
	}

	LOG_DEBUG("Translating achievements");
    // 5. Cargar logica del juego
    int system = translateSystemAchievement();
    std::string pathToRom = unzipped.extractedPath.empty() ? unzipped.originalPath : unzipped.extractedPath;
	LOG_DEBUG("Loading achievements from game");
    Achievements::instance()->load_game((uint8_t *)unzipped.memoryBuffer, unzipped.romsize, pathToRom, system, messagesAchievement);
	
}

std::string GameMenu::configButtonsJOY(){
    bool salir = false;
    string salida = "";
	Uint32 bkgText = SDL_MapRGB(this->overlay->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);
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
		
		SDL_FillRect(this->overlay, NULL, bkgText);
		Constant::drawTextCent(this->overlay, Fonts::getFont(Fonts::FONTSMALL), FRONTEND_BTN_TXT[i].c_str(), 0, 20, true, false, textColor, 0);
        SDL_Flip(this->gameScreen);
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
    const int snapW = overlay->w / 2;
    const int snapH = listMenu.getH() / 2;
    const int snapOffset = overlay->w / 10;
    //const int snapOffset = 5;
    menuImages.clear();
    menuTextAreas.clear();

    if (overlay->w / 2 >= 320){
        imageSnap.setX(overlay->w / 2 + snapOffset);
        imageSnap.setY(listMenu.getY());
        imageSnap.setW(snapW - snapOffset * 2);
        imageSnap.setH(snapH - snapOffset);
    } else {
        imageSnap.setX(overlay->w / 2);
        imageSnap.setY(listMenu.getY());
        imageSnap.setW(snapW);
        imageSnap.setH(snapH);
    }
    imageSnap.vAlign = ALIGN_TOP;
    menuImages.insert(make_pair(SNAP, imageSnap));

    /** Box2d */
    Image imageBox2d;
    const int box2dH = listMenu.getH() / 4;
    const int box2dW = overlay->w / 8;
    imageBox2d.setX(overlay->w / 2);
    imageBox2d.setY(overlay->h / 2 - box2dH);
    imageBox2d.setW(box2dW);
    imageBox2d.setH(box2dH);
    menuImages.insert(make_pair(BOX2D, imageBox2d));

    /** snaptit*/
    Image imageSnaptit;
    const int snapTitH = listMenu.getH() / 4;
    const int snapTitW = overlay->w / 6;
    imageSnaptit.setX(overlay->w - snapTitW);
    imageSnaptit.setY(overlay->h / 2 - snapTitH);
    imageSnaptit.setW(snapTitW);
    imageSnaptit.setH(snapTitH);
    menuImages.insert(make_pair(SNAPTIT, imageSnaptit));

    Image imageSnapFs(0, 0, overlay->w, overlay->h);
    imageSnapFs.drawfaded = true;
    menuImages.insert(make_pair(SNAPFS, imageSnapFs));
    
    const int sectionGap = 0;
    const int textAreaY = listMenu.getH() / 2 + listMenu.getY() + sectionGap;
    TextArea textarea(overlay->w / 2, textAreaY, overlay->w / 2, overlay->h - textAreaY);
    textarea.marginX = (int)floor((double)overlay->w / 100);
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
    const int halfWidth = overlay->w / 2;
	int face_h_big = TTF_FontLineSkip(fontBig);
	int face_h_small = TTF_FontLineSkip(fontsmall);
	bool debug = true;

    //Drawing the rest of list and images
    if (listMenu.getNumGames() > (std::size_t)listMenu.curPos){
        auto game = listMenu.listGames.at(listMenu.curPos).get();
        
        if (!game->shortFileName.empty()){
            if (listMenu.layout == LAYBOXES) {
				Constant::drawTextCentTransparent(overlay, fontBig, emu.name.c_str(), 0, face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
					true, false, textColor, 0);
				showScrapProcess(listMenu);
				fastline(this->overlay, listMenu.marginX, listMenu.marginY - 1 , overlay->w - listMenu.marginX, listMenu.marginY - 1, menuBars);
                fastline(this->overlay, sepVertX, listMenu.marginY , sepVertX, listMenu.getH() + listMenu.marginY - 1, menuBars);
                listMenu.draw(this->overlay);

                //Draw and update the overlay because the loading of images can take a long time
                if (listMenu.keyUp){
                    string assetsDir = dirutil::getPathPrefix(emu.assets) + string(Constant::tempFileSep);
                    //Drawing the rom's synopsis text
                    menuTextAreas[SYNOPSIS].loadTextFileFromGame(assetsDir + "synopsis" + string(Constant::tempFileSep), *game, ".txt");
                    menuTextAreas[SYNOPSIS].resetTicks(this->gameTicks);
                    menuTextAreas[SYNOPSIS].draw(this->overlay, this->gameTicks);

                    //Snapshot picture
                    menuImages[SNAP].loadImageFromGame(assetsDir + "snap" + string(Constant::tempFileSep), *game, ".png");
                    menuImages[SNAP].printImage(this->overlay);

                    if (overlay->w < 640){
                        //If it's so small, only show the snapshot
                        return;
                    }

                    //Box picture
                    menuImages[BOX2D].loadImageFromGame(assetsDir + "box2d" + string(Constant::tempFileSep), *game, ".png");
                    menuImages[BOX2D].printImage(this->overlay);

                    //Title picture
                    menuImages[SNAPTIT].loadImageFromGame(assetsDir + "snaptit" + string(Constant::tempFileSep), *game, ".png");
                    menuImages[SNAPTIT].printImage(this->overlay);
                } else {
                    menuImages[SNAP].printImage(this->overlay);
                    menuImages[BOX2D].printImage(this->overlay);
                    menuImages[SNAPTIT].printImage(this->overlay);
                    menuTextAreas[SYNOPSIS].draw(this->overlay, this->gameTicks);
                }

            } else if (listMenu.layout == LAYSIMPLE) {
                if (listMenu.keyUp){
                    //Snapshot picture
                    menuImages[SNAPFS].loadImageFromGame(dirutil::getPathPrefix(emu.assets) + string(Constant::tempFileSep)
                        + "snap" + string(Constant::tempFileSep), *game, ".png");
                }
                menuImages[SNAPFS].printImage(this->overlay);
                //Draw the menu element after the image
                Constant::drawTextCent(overlay, fontBig, emu.name.c_str(), 
					halfWidth, face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
					true, false, textColor, 0);

                fastline(this->overlay, listMenu.marginX, listMenu.marginY - 1, listMenu.getW(), listMenu.marginY - 1, textColor);
                listMenu.draw(this->overlay);

            } else if (listMenu.layout == LAYTEXT) {

				Constant::drawTextCent(overlay, fontBig, emu.name.c_str(), 
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
		//Constant::drawTextCent(overlay, fontBig, emu.name.c_str(), 
		//			cfgLoader->getWidth(), face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
		//			true, false, textColor, 0);

		Constant::drawTextCentTransparent(overlay, fontBig, emu.name.c_str(), 0, face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
					true, false, textColor, 0);

        fastline(this->overlay, listMenu.marginX, listMenu.marginY - 1, overlay->w - listMenu.marginX, listMenu.marginY - 1, textColor);
		Constant::drawTextCent(overlay, fontsmall, "No roms found", 0, 0, true, true, textColor, 0);

		// Para renderizar, usas el puntero de la clase
        Menu* m = configMenus->obtenerMenuActual();
        // Dibujar m->titulo y m->opciones[i]->titulo...

    } else {
		Constant::drawTextCent(overlay, fontsmall, "The configuration is not valid", 0, 0, true, true, textColor, 0);
		Constant::drawTextCent(overlay, fontsmall, "Press TAB to select the next entry or", 0, face_h_small + 3, true, true, textColor, 0);
		Constant::drawTextCent(overlay, fontsmall, "Press ESC to exit", 0, (face_h_small + 3) * 2, true, true, textColor, 0);
    }
}

/**
*
*/
void GameMenu::showScrapProcess(ListMenu &listMenu){
	TTF_Font *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
    const int halfWidth = overlay->w / 2;
	int face_h_small = TTF_FontLineSkip(fontsmall);

	if (Scrapper::isScrapping()){
		Scrapper::g_status.procesados;
		std::string str = "Scrapping: " + Constant::TipoToStr(Scrapper::g_status.procesados) + "/" + Constant::TipoToStr(Scrapper::g_status.total); 
		if (Scrapper::g_status.remainingMedia > 0){
			str += " - " + LanguageManager::instance()->get("msg.download.media") + " " + Constant::TipoToStr(Scrapper::g_status.remainingMedia);
		}
		std::string str2 = std::string(Scrapper::g_status.emuActual) + " - " + std::string(Scrapper::g_status.juegoActual);
		Constant::drawTextTransparent(overlay, fontsmall, str.c_str(), halfWidth + halfWidth / 4, face_h_small < listMenu.marginY ? (listMenu.marginY - face_h_small) / 2 - face_h_small / 2 : 0 , textColor, 0);
		Constant::drawTextTransparent(overlay, fontsmall, str2.c_str(), halfWidth + halfWidth / 4, face_h_small < listMenu.marginY ? (listMenu.marginY - face_h_small) / 2 + face_h_small / 2: 0 , textColor, 0);
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
    int startGray = 240;
    static const int bkg = SDL_MapRGB(this->overlay->format, startGray, startGray, startGray);
    TTF_Font *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
	int face_h_small = TTF_FontLineSkip(fontsmall);
    
    int rw = Fonts::getSize(Fonts::FONTSMALL, msg.c_str()) + 5; 
    //int rh = this->overlay->h / 3;
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
	Constant::drawTextCent(overlay, fontsmall, msg.c_str(), 
		this->overlay->w / 2, this->overlay->h / 2, true, true, black, -1);
}

/**
 * 
 */
void GameMenu::loadEmuCfg(ListMenu &menuData){
    TTF_Font *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
	int face_h_small = TTF_FontLineSkip(fontsmall);
	static const int cblack = SDL_MapRGB(this->overlay->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);

    if (cfgLoader->emulators.size() == 0){
        SDL_FillRect(overlay, NULL, cblack);
        string msg = "There are no emulators configured. Exiting..."; 
		Constant::drawTextCent(overlay, fontsmall, msg.c_str(), 0, 0, true, true,  white, -1);
		Constant::drawTextCent(overlay, fontsmall, "Press a key to continue", 0, face_h_small + 3, true, true, white, -1);
		SDL_Flip(gameScreen);
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
        mapfilepath = dirutil::getPathPrefix(emu->rom_directory);
        vector<unique_ptr<FileProps>> files;
		
		string extFilter = " " + emu->rom_extension;
        extFilter = Constant::replaceAll(extFilter, " ", ".");

		if (isDebug()){
            SDL_FillRect(overlay, NULL, cblack);
            string msg = "searching " + mapfilepath; 
			Constant::drawTextCent(overlay, fontsmall, msg.c_str(), overlay->w / 2, overlay->h / 2, true, true,  white, -1);
        }

        dir.listarFilesSuperFast(mapfilepath.c_str(), files, extFilter, true, false);

		ConfigEmu emu = *cfgLoader->getCfgEmu();
        string mapfilepath = dirutil::getPathPrefix(emu.rom_directory);

        if (isDebug()){
            SDL_FillRect(overlay, NULL, cblack);
            string msg = "roms found: " + Constant::TipoToStr(files.size()); 
            string msg2 = "In dir " + mapfilepath;
			Constant::drawTextCent(overlay, fontsmall, msg.c_str(), 0, 0, true, true,  white, -1);
            Constant::drawTextCent(overlay, fontsmall, msg2.c_str(), 0, face_h_small + 3, true, true,  white, -1);
			Constant::drawTextCent(overlay, fontsmall, "Press a key to continue", 0, (face_h_small + 3) * 2, true, true,  white, -1);
            SDL_Delay(3000);
        }

        menuData.filesToList(files, emu);
        files.clear();
    }
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
	string pathPrefix = dirutil::getPathPrefix(emu.directory);
	
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
        string romdir = emu.use_rom_directory ? dirutil::getPathPrefix(emu.rom_directory) + string(Constant::tempFileSep) : "";
        string romFile = game->longFileName;
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

    if (!emu.options_before_rom){
        commands.emplace_back(emu.global_options);
    }

	std::string firstCommand = commands.at(0);
	LOG_DEBUG("Launching %s\n", firstCommand.c_str());
	#ifdef LIBRETRO
		Launcher launcher;
		this->running = !launcher.launch(commands, isDebug());
	#endif

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
        LOG_ERROR("Error Writing to File: %s", filepath.c_str());
        return 1;
    }

    struct ListStatus input1 = { cfgLoader->emuCfgPos, menuData.iniPos, menuData.endPos, 
        menuData.curPos, menuData.maxLines, menuData.layout, menuData.animateBkg};

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
    if (*this->mustUpdateFps) {
        uint32_t currentTick = SDL_GetTicks();
        
        // Temporizadores independientes
        shouldUpdateFps = (currentTick - lastFpsUpdate > 500) || fpsSurface == NULL;
		//const bool shouldUpdateFps = (currentTick - lastFpsUpdate > 500);
        const bool shouldUpdateMem = (currentTick - lastMemUpdate > 5000) || memSurface == NULL;
		int lastCpuW = 0;

        // 1. Actualización de contadores internos
        this->sync->update_fps_counter(shouldUpdateFps, currentTick);

        // 2. Lógica para FPS y CPU (cada 500ms)
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
			
            fpsSurface = TTF_RenderUTF8(Fonts::getFont(Fonts::FONTSMALL), this->sync->fpsText, white, black);
            cpuSurface = TTF_RenderUTF8(Fonts::getFont(Fonts::FONTSMALL), this->sync->cpuText, white, black);
            
            lastFpsUpdate = currentTick;
        }

        // 3. Lógica para MEMORIA (cada 5000ms / 5 segundos)
		if (shouldUpdateMem) {
			if (memSurface) SDL_FreeSurface(memSurface);
			
			double availMB = 0;
			double totalPercent = 0;
#ifdef _XBOX
			MEMORYSTATUS ms;
			ms.dwLength = sizeof(ms);
			GlobalMemoryStatus(&ms);
			availMB = (double)ms.dwAvailPhys / (1024.0 * 1024.0);
			// Cálculo manual del porcentaje: (Usado / Total) * 100
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
			// Usamos double para mayor precisión en los cálculos de GB
			if (availMB >= 1024.0) {
				sprintf(memText, "MEM: %.0f%% (%.2fGB Free)", totalPercent, availMB / 1024.0);
			} else {
				sprintf(memText, "MEM: %.0f%% (%.0fMB Free)", totalPercent, availMB);
			}

			memSurface = TTF_RenderText(Fonts::getFont(Fonts::FONTSMALL), memText, white, black);
			lastMemUpdate = currentTick;
		}

        // 4. DIBUJO (En cada frame, usando las superficies actuales)
        if (fpsSurface && cpuSurface && memSurface) {
            // Dibujar MEM (Posición relativa a CPU)
            SDL_Rect rectMem = {this->overlay->w - memSurface->w -3, 1, memSurface->w, memSurface->h};
			clearOverlayRect(rectMem);
            SDL_BlitSurface(memSurface, NULL, this->overlay, &rectMem);

            // Dibujar CPU (Posición relativa a FPS)
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
	int modeOk = true;
	int startingMode = *this->current_scaler_mode;

	struct retro_system_av_info av_info;
	retro_get_system_av_info(&av_info);
	const unsigned ancho_base = av_info.geometry.base_width;
	const unsigned alto_base = av_info.geometry.base_height;
	std::string msgShader;
	std::string choosenFilter;

	switch (hotkey){
		case HK_RATIO:
			*this->current_ratio = (*this->current_ratio + 1) % TOTAL_VIDEO_RATIO;
			showSystemMessage(aspectRatioStrings[*this->current_ratio], 3000);
			break;

		case HK_SHADER:
			#ifdef _XBOX
				*this->current_shader = (*this->current_shader + 1) % TOTAL_SHADERS;
				XBOX_SelectEffect(*current_shader);
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
					} else if (*this->current_force_fs && (*current_scaler_mode == SCALE4X || *current_scaler_mode == SCALE3X 
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
	}
	//joystick->resetButtonsCore();
}

/**
*
*/
SDL_Surface* GameMenu::clonarPantalla(SDL_Surface* src, int transparency) {
    if (!src) return NULL;
	SDL_Surface* copia = NULL;
	//Dimensiones y cálculo de aspecto
	Dimension srcDim = {src->w, src->h};
	Dimension dstDim = {overlay->w, overlay->h};
	Dimension resDim = Image::relacion(srcDim, dstDim);
	Dimension resCen = Image::centrado(resDim, dstDim);
	SDL_Rect dstRect = {resCen.w , resCen.h, (Uint16)resDim.w, (Uint16)resDim.h};

	if (resDim.w == src->w && resDim.h == src->h){
		copia = SDL_CreateRGBSurface(SDL_SWSURFACE, overlay->w, overlay->h, 32, 
		rmask, gmask, bmask, amask);
		SDL_BlitSurface(src, NULL, copia, NULL);
		SDL_SetAlpha(copia, SDL_SRCALPHA, 0);
		boxRGBA(copia, 0, 0, copia->w -1, copia->h -1, colors[clBackground].sdlColor.r, colors[clBackground].sdlColor.g, colors[clBackground].sdlColor.b, transparency);
	} else {
		SDL_Surface *tmp = SDL_CreateRGBSurface(SDL_SWSURFACE, overlay->w, overlay->h, 
			src->format->BitsPerPixel, src->format->Rmask, src->format->Gmask, 
			src->format->Bmask, src->format->Amask);

		SDL_SoftStretch(src, NULL, tmp, &dstRect);

		if (transparency < 255) {
			boxRGBA(tmp, 0, 0, tmp->w - 1, tmp->h - 1, 
					colors[clBackground].sdlColor.r, 
					colors[clBackground].sdlColor.g, 
					colors[clBackground].sdlColor.b, 
					transparency);
		}
		copia = SDL_DisplayFormat(tmp);
		SDL_FreeSurface(tmp);
	}
	
    return copia;
}

void GameMenu::selectScalerMode(int mode){
#ifndef _XBOX
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
    Message msg;
    msg.content = text;
    msg.timeout = duration;
    msg.ticks = SDL_GetTicks();
    
    std::string newText = Fonts::recortarAlTamanyo(text, this->overlay->w);
	msg.cache = TTF_RenderUTF8_Blended(Fonts::getFont(Fonts::FONTBIG), newText.c_str(), white);
    
    if (msg.cache) {
        int face_h = TTF_FontLineSkip(Fonts::getFont(Fonts::FONTBIG));
        msg.rect.x = 0;
        // La posición Y se calculará dinámicamente al dibujar para que se apilen
        msg.rect.w = msg.cache->w + 2;
        msg.rect.h = face_h + 4;
        messages.push_back(msg);
    }
}

void GameMenu::renderTrackers() {
    Achievements* ach = Achievements::instance();
    if (ach->trackers.empty()) return;

    // No necesitamos bloquear aquí porque el método .render() interno ya lo hace
    TTF_Font* font = Fonts::getFont(Fonts::FONTSMALL);
    int margin = 20;
    int posX = overlay->w - margin; // Punto de anclaje derecho
    int posY = margin;

    // Llamamos al proceso seguro
    ach->trackers.render(this->overlay, font, posX, posY);
}


void GameMenu::renderChallenges() {
    Achievements* ach = Achievements::instance();
    if (ach->challenges.empty()) return;
	ach->challenges.render(this->overlay, 0);
}

void GameMenu::renderProgress() {
	Achievements::instance()->progress.render(overlay, 0, colors[clBackground]);
}

void GameMenu::processMessagesAchievements(){
	if (!getCfgLoader()->configMain[cfg::enableAchievements].valueBool)
		return;

	const uint32_t currentTicks = SDL_GetTicks();

	// 2. Solo procesar si realmente ha pasado tiempo (Throttle)
    // No necesitas actualizar la lógica de logros cada 16ms (60fps). 
    // Hacerlo cada 33ms o 100ms libera muchísima CPU.
    static uint32_t lastUpdate = 0;
    if (currentTicks - lastUpdate > 33) { 
		// Actualizar estado interno de los logros
		updateAchievementsState(currentTicks);
		// Gestión de la cola de mensajes (Expiración y carga)
		handleMessageQueue(currentTicks);
		lastUpdate = currentTicks;
	}
    
	// 3. Renderizado condicional: Solo entra si hay algo que dibujar
	// Renderizado (Si hay mensajes)
	if (messagesAchievement.empty()) {
		if (lastMessagesArea.h > 0) {
			clearOverlayRect(lastMessagesArea);
			lastMessagesArea.x = 0;
			lastMessagesArea.y = 0;
			lastMessagesArea.w = 0;
			lastMessagesArea.h = 0;
		}
    } else {
		renderCurrentAchievement();
	}
    
	// Renderizado de trackers
	renderTrackers();
	// Renderizado de challenges
	renderChallenges();
	// Renderizado de progresos
	renderProgress();
}

void GameMenu::clearLastAchievementArea() {
    // Solo limpiamos si el área tiene dimensiones válidas
    if (lastMessagesArea.w > 0 && lastMessagesArea.h > 0) {
        // Pintamos un rectángulo del color de fondo (uBkgColor) 
        // sobre el área que ocupaba el último logro
		clearOverlayRect(lastMessagesArea);
    }
}

inline void GameMenu::updateAchievementsState(uint32_t currentTicks) {
    if (getEmuStatus() == EMU_STARTED) {
		Achievements* ach = Achievements::instance();
        ach->doFrame();
        // Load new messages to the list
		while (!ach->messages.empty()) {
			AchievementState msg;
			if (ach->messages.pop_with_new_surfaces(msg)){
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
                               msg->badge, lastMessagesArea);
    } else if (msg->type == ACH_UNLOCKED){
		Achievements::instance()->setShouldRefresh(true);
        showAchievementMessage(LanguageManager::instance()->get("msg.achievement.unlocked.title"), msg->title, msg->description, msg->badge, lastMessagesArea);
    } else if (msg->type == ACH_WARNING){
        showAchievementMessage(LanguageManager::instance()->get("msg.achievement.warning.title"), msg->title, msg->description, msg->badge, lastMessagesArea);
    }
}

void GameMenu::showAchievementMessage(std::string line1Str, std::string line2Str, std::string line3Str, SDL_Surface *badge, SDL_Rect& lastMessagesArea){
	SDL_Surface *line1 = TTF_RenderUTF8_Blended(Fonts::getFont(Fonts::FONTSMALL), line1Str.c_str(), white);
	SDL_Surface *line2 = TTF_RenderUTF8_Blended(Fonts::getFont(Fonts::FONTSMALL), line2Str.c_str(), yellow);
	SDL_Surface *line3 = TTF_RenderUTF8_Blended(Fonts::getFont(Fonts::FONTSMALL), line3Str.c_str(), blue);

	int maxW = line1->w > line2->w ? line1->w : line2->w;
	maxW = maxW > line3->w ? maxW : line3->w; 
	const int paddingBottom = 10;
	int line_height, badgeW, badgeH, badgePad;

	Achievements& self = *Achievements::instance();

	Fonts::getBadgeSize(badgeW, badgeH, badgePad, line_height);
	const int maxH = this->overlay->h -paddingBottom -line_height * 3;

	SDL_Rect rect = {10 + badgeW, 0, 0, line_height};
	lastMessagesArea.x = rect.x - badgeW;
	lastMessagesArea.y = maxH;
	lastMessagesArea.w = maxW + badgeW + badgePad * 3;
	lastMessagesArea.h = this->overlay->h - maxH - paddingBottom;

	const Uint32 uPaleblue = SDL_MapRGB(this->overlay->format, paleblue.r, paleblue.g, paleblue.b);
	SDL_FillRect(this->overlay, &lastMessagesArea, uPaleblue);
	//DrawRectAlpha(overlay, lastMessagesArea, black, 230);

	SDL_Rect txtRect = {rect.x + badgePad * 2, maxH, 0, line1->w};
	SDL_BlitSurface(line1, NULL, this->overlay, &txtRect);

	txtRect.y = this->overlay->h -paddingBottom -line_height * 2;
	txtRect.w = line2->w;
	SDL_BlitSurface(line2, NULL, this->overlay, &txtRect);

	txtRect.y = this->overlay->h -paddingBottom -line_height;
	txtRect.w = line3->w;
	SDL_BlitSurface(line3, NULL, this->overlay, &txtRect);
	
	SDL_FreeSurface(line1);
	SDL_FreeSurface(line2);
	SDL_FreeSurface(line3);

	if (badge != NULL){
		SDL_Rect rectBadge = {rect.x - badgeW + badgePad, this->overlay->h -paddingBottom -line_height * 3 + badgePad, 0, line_height};
		SDL_BlitSurface(badge, NULL, this->overlay, &rectBadge);
	}
}

/**
*
*/
void GameMenu::processMessages() {
    if (messages.empty()) return;

    // 1. LIMPIEZA TOTAL: Antes de mover nada, borramos la zona donde suelen estar
    // (Opcional: puedes calcular un rect global que cubra todos los mensajes)
    for (std::size_t i = 0; i < messages.size(); ++i) {
        clearOverlayRect(messages[i].rect); 
    }

    // 2. ACTUALIZACIÓN: Eliminar mensajes caducados
    uint32_t currentTicks = SDL_GetTicks();
    for (int i = (int)messages.size() - 1; i >= 0; i--) {
        if (currentTicks - messages[i].ticks > messages[i].timeout) {
            if (messages[i].cache) SDL_FreeSurface(messages[i].cache);
            messages.erase(messages.begin() + i);
        }
    }

    if (messages.empty()) return;

    // 3. CÁLCULO DE POSICIONES Y DIBUJO
    static int line_height = TTF_FontLineSkip(Fonts::getFont(Fonts::FONTBIG)) + 4;
    int currentY = this->overlay->h - line_height;

    // Usamos referencia directa en el bucle para mayor seguridad que el puntero mData
    for (int i = (int)messages.size() - 1; i >= 0; --i) {
        Message &m = messages[i];
        
        // Actualizamos la nueva posición
        m.rect.x = 0;
        m.rect.y = (Sint16)currentY;

        if (m.cache) {
            m.rect.w = (Uint16)m.cache->w;
            m.rect.h = (Uint16)m.cache->h;
            
            // Dibujamos fondo y texto
            SDL_FillRect(overlay, &m.rect, colors[clBackground].color);
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

	//Loading the joystick configuration if exists
	std::string ruta = dir.getFolder(rp) + Constant::getFileSep() + dir.getFileNameNoExt(rp) + CFG_EXT;

	std::string coreDefaultsPath = Constant::getAppDir() + std::string(Constant::tempFileSep) + "config"
		+ std::string(Constant::tempFileSep) + PREFIX_DEFAULTS + coreName + CFG_EXT;

	if (dir.fileExists(ruta.c_str())){
		joystick->loadButtonsRetro(ruta);
	} else if (dir.fileExists(coreDefaultsPath.c_str())){
		joystick->loadButtonsRetro(coreDefaultsPath);
	} else {
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
	//SDL_FillRect(overlay, NULL, colors[clBackground].color);
}

void GameMenu::clearOverlayRect(SDL_Rect& rect){
	//SDL_FillRect(overlay, &rect, colors[clBackground].color);
	SDL_FillRect(overlay, &rect, 0);
}

void GameMenu::fillOverlay(int colorIndex){
	if (colorIndex < clTotalColors){
		SDL_FillRect(this->overlay, NULL, colors[colorIndex].color);
	}
}

void GameMenu::fillOverlayAlpha(int colorIndex, int alpha){
	if (colorIndex < clTotalColors){
		const SDL_Color& col = colors[colorIndex].sdlColor;
		const Uint32 colorA = SDL_MapRGBA(this->overlay->format, col.r, col.g, col.b, alpha);
		SDL_FillRect(this->overlay, NULL, colorA);
	}
}