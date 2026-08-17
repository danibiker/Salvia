#include <algorithm> // Imprescindible para std::sort
#include <math.h>
#include <sstream>

#include <menus/gestormenus.h>
#include <const/constant.h>
#include <const/menuconst.h>
#include <gfx/gfx_utils.h>
#include <gfx/SDL_gfxPrimitives.h>
#include <io/joystick.h>
#include <image/icons.h>
#include <utils/langmanager.h>
#include <http/httputil.h>
#include <http/achievements.h>
#include <so/soutils.h>
#include <cheats/cheatmanager.h>
#include <video/HLSLBackground.h>


SDL_Surface* GestorMenus::imgText;

std::string syncOptionsStrings[TOTAL_VIDEO_SYNC];
std::string aspectRatioStrings[TOTAL_VIDEO_RATIO];
std::string videoScaleStrings[TOTAL_VIDEO_SCALE];
std::string videoIntScaleStrings[TOTAL_INT_SCALE];
std::string videoShaderStrings[TOTAL_SHADERS];
std::string ACTION_ASK_STR[MAX_ASK];
std::string TipoKeyStr[KEY_JOY_MAX];
std::string configurablePortButtonsStr[MAXJOYBUTTONS];
std::string configurablePortHatsStr[MAXJOYBUTTONS];
std::string HOTKEYS_STR[HK_MAX];

extern bool swapToNewDisc(const std::string& newBinPath);
extern bool swapDisc(unsigned new_idx);
extern struct retro_disk_control_callback disk_control;
extern void launchBios();
extern std::string downloadCheatWithProgress();
extern std::string lastCheatSourceName();

const char *scrapOrigins[] = {"SCREENSCRAPER", "THEGAMESDB", "EMPTY"};

GestorMenus::GestorMenus(int screenw, int screenh){
	face_h_big = Fonts::getLineSkip(Fonts::FONTBIG);
	face_h_small = Fonts::getLineSkip(Fonts::FONTSMALL);
	menuRaiz = NULL;
	menuActual = NULL;
	setObjectType(GUIOPTIONS);
	iniPos = 0;
    endPos = 0;
    curPos = 0;
    listSize = 0;
    maxLines = 0;
    marginX = (int)floor((double)(screenw / 100));
    marginY = face_h_big * 2;
    lastSel = -1;
    pixelShift = 0;
	this->setLayout(0, screenw, screenh);
	status = NORMAL;
	options_changed_flag = false;

	const int box2dW = screenw / 2 - 2 * marginX;
	const int box2dH = box2dW;
    
	imageSavestate.setX(screenw - box2dW - marginX);
    imageSavestate.setY(getY() + 3);
    imageSavestate.setW(box2dW);
	imageSavestate.setH(box2dH);

	imageFaq.setX(getX());
	imageFaq.setY(getY());
	imageFaq.setW(getW() - marginX);
	imageFaq.setH(getH());

	askNumOptions = 0;
	scrapGamesSelection = 1;

	tmpTextOption = NULL;
}

GestorMenus::~GestorMenus() {
	for (std::size_t i = 0; i < cdromListMenu->opciones.size(); ++i) {
		delete ((OpcionTxt *)cdromListMenu->opciones[i])->context;
	}

	// Transient per-category core-option submenus (not in todosLosMenus).
	for (std::size_t i = 0; i < coreOptionSubmenus.size(); ++i) {
		delete coreOptionSubmenus[i];
	}
	coreOptionSubmenus.clear();

    for(std::size_t i = 0; i < todosLosMenus.size(); i++) {
		if (todosLosMenus[i])
			delete todosLosMenus[i];
		todosLosMenus[i] = NULL;
	}

	if (tmpTextOption != NULL){
		SDL_FreeSurface(tmpTextOption);
	}

	imageSavestate.closeImage();
	imageFaq.closeImage();
}

std::string GestorMenus::guardarJoysticks(Joystick* joy){
	LOG_DEBUG("Guardando valores del joystick");
	return LanguageManager::instance()->get("msg.filesave") + joy->saveButtonsRetroDefault();
}

std::string GestorMenus::guardarGameJoysticks(Joystick* joy){
	LOG_DEBUG("Guardando valores del joystick para el juego");
	std::string msg = joy->saveButtonsRetroGame();
	if (msg.empty()){
		return LanguageManager::instance()->get("msg.key.cfg.load");
	} else {
		return LanguageManager::instance()->get("msg.filesave") + msg;
	}
}

std::string GestorMenus::guardarCoreJoysticks(Joystick* joy){
	LOG_DEBUG("Guardando valores del joystick para el core");
	std::string msg = joy->saveButtonsRetroCore();
	return LanguageManager::instance()->get("msg.filesave") + msg;
}

std::string GestorMenus::guardarCoreConfig(CfgLoader *refConfig){
	LOG_DEBUG("Guardando valores del core actual");
	return refConfig->saveCoreParams();
}

// Restaura TODAS las opciones del core (core + game-specific) a su valor por
// defecto. El default lo declara el core en el parse (applyEntry -> defaultSelected).
// setParameter sirve values[selected] en GET_VARIABLE, asi que basta reponer
// selected (+ cachedValue para coherencia inmediata) y marcar el cambio para que
// el core lo relea. Persistimos para que el reset sobreviva a recargas.
// NOTA: las opciones init-only (threading/widescreen/gpu_renderer) quedan en su
// default pero solo surten efecto al recargar el core.
std::string GestorMenus::restaurarCoreConfig(CfgLoader *refConfig){
	LOG_DEBUG("Restaurando opciones del core a sus valores por defecto");
	for (auto it = refConfig->startupLibretroParams.begin();
	     it != refConfig->startupLibretroParams.end(); ++it) {
		cfg::t_emu_props *p = it->second.get();
		int d = (p->defaultSelected >= 0 && p->defaultSelected < (int)p->values.size())
		        ? p->defaultSelected : 0;
		p->selected = d;
		if (!p->values.empty()) p->cachedValue = p->values[d];
	}
	for (auto it = refConfig->gameSpecificLibretroParams.begin();
	     it != refConfig->gameSpecificLibretroParams.end(); ++it) {
		cfg::t_emu_props *p = it->second.get();
		int d = (p->defaultSelected >= 0 && p->defaultSelected < (int)p->values.size())
		        ? p->defaultSelected : 0;
		p->selected = d;
		if (!p->values.empty()) p->cachedValue = p->values[d];
	}
	options_changed_flag = true;   // el core relee en el proximo GET_VARIABLE_UPDATE
	return LanguageManager::instance()->get("menu.core.options.restore.applied");
}

std::string GestorMenus::guardarMainConfig(CfgLoader *refConfig){
	LOG_DEBUG("Guardando valores principales de configuracion");
	return refConfig->saveMainParams();
}

std::string GestorMenus::guardarCoreOverridesConfig(t_save_override *overrides){
	LOG_DEBUG("Guardando overrides del core actual");
	return overrides->refConfig->saveCoreOverrideParams(overrides->emuIdx);
}

std::string GestorMenus::volverEmulacion(CONFIG_STATUS *st){
	*st = EXIT_CONFIG;
	return std::string("");
}

std::string GestorMenus::salirEmulacion(CONFIG_STATUS *st){
	*st = EXIT_EMULATION;
	return std::string("");
}

std::string GestorMenus::startScrapping(CONFIG_STATUS *st){
	bool someSelected = false;
	for (std::size_t i=0; i < scrapSelection.size() && !someSelected; i++){
		someSelected = scrapSelection[i].selected;
	}

	if (someSelected){
		*st = START_SCRAPPING;
		if (menuScrapper->opciones.size() > 0) {
			// Obtener el ultimo elemento
			auto* baseOpt = menuScrapper->opciones.back();
			OpcionExec<CONFIG_STATUS>* opcion = static_cast<OpcionExec<CONFIG_STATUS>*>(baseOpt);
			if (opcion != nullptr) {
				opcion->titulo = LanguageManager::instance()->get("menu.scrap.stop");
				opcion->execfunc = &GestorMenus::stopScrapping;
			}
		}
		return std::string("");
	} else {
		LOG_DEBUG("Seleccione al menos un sistema que escrapear");
		return LanguageManager::instance()->get("msg.atleast1scrap");
	}
}

std::string GestorMenus::stopScrapping(CONFIG_STATUS *st){
	if (st != NULL){
		*st = NORMAL;
	}
	if (menuScrapper->opciones.size() > 0) {
		// Obtener el ultimo elemento
		auto* baseOpt = menuScrapper->opciones.back();
		OpcionExec<CONFIG_STATUS>* opcion = static_cast<OpcionExec<CONFIG_STATUS>*>(baseOpt);
		if (opcion != nullptr) {
			InterlockedExchange(&CurlClient::g_abortScrapping, 1);
			opcion->titulo = LanguageManager::instance()->get("menu.scrap.start");
			opcion->execfunc = &GestorMenus::startScrapping;
		}
	}
	return std::string("");
}


void GestorMenus::setLayout(int layout, int screenw, int screenh){
	this->marginY = face_h_big * 2;
    clearSelectedText();
  
    this->setX(marginX);
    this->setY(marginY);
    this->setW(screenw - marginX);
    this->setH(screenh - marginY);
    this->centerText = false;
    this->layout = layout;
}

// Inicializa la estructura de menus
void GestorMenus::inicializar(CfgLoader *refConfig, Joystick *joystick) {
	SDL_JOY_TO_XBOX[0] = LanguageManager::instance()->get("menu.controls.left");
	SDL_JOY_TO_XBOX[1] = LanguageManager::instance()->get("menu.controls.right");
	SDL_JOY_TO_XBOX[2] = LanguageManager::instance()->get("menu.controls.up");
	SDL_JOY_TO_XBOX[3] = LanguageManager::instance()->get("menu.controls.down");

	SDL_HAT_TO_XBOX[1] = LanguageManager::instance()->get("menu.controls.up");
	SDL_HAT_TO_XBOX[2] = LanguageManager::instance()->get("menu.controls.right");
	SDL_HAT_TO_XBOX[4] = LanguageManager::instance()->get("menu.controls.down");
	SDL_HAT_TO_XBOX[8] = LanguageManager::instance()->get("menu.controls.left");

	// 1. Crear contenedores de menus
    menuRaiz = new Menu(LanguageManager::instance()->get("menu.main.options"));
    Menu* menuVideo = new Menu(LanguageManager::instance()->get("menu.main.video"), menuRaiz);
	Menu* menuEmulation = new Menu(LanguageManager::instance()->get("menu.main.emulation"), menuRaiz);
	Menu* menuEntrada = new Menu(LanguageManager::instance()->get("menu.main.input"), menuRaiz);
	menuCoreOptions = new Menu(LanguageManager::instance()->get("menu.main.core.options"), menuRaiz);
	menuCheats = new Menu(LanguageManager::instance()->get("menu.main.cheats"), menuRaiz);
	menuSavestates = new Menu(LanguageManager::instance()->get("menu.main.saves"), face_h_big, this->getW() / 2 - 2 * marginX, menuRaiz);
	menuScrapper = new Menu(LanguageManager::instance()->get("menu.main.scrapper"), menuRaiz);
	
	Menu* parentAchievements = new Menu(LanguageManager::instance()->get("menu.achievement.title"), menuRaiz);
	//Creamos el submenu que contiene la lista de logros
	const int rowAchHeight = face_h_big * 2;
	const int menuAchWidth = this->getW() - marginX;
	menuAchievements = new Menu(LanguageManager::instance()->get("menu.achievement.list.title"), rowAchHeight, menuAchWidth, parentAchievements);
	OpcionSubMenu *listaLogros = new OpcionSubMenu(LanguageManager::instance()->get("menu.achievement.list.title"), menuAchievements);
	listaLogros->callback = &GestorMenus::sDescargarLogros;
    listaLogros->context = this;
	parentAchievements->opciones.push_back(listaLogros);
	//Incluimos un indicador para habilitar logros
	OpcionBool *opcionEnableAchievements = new OpcionBool(LanguageManager::instance()->get("menu.achievement.enable"), &refConfig->configMain[cfg::enableAchievements].getBoolRef());
	opcionEnableAchievements->callback = &GestorMenus::changeEnableAchievements;
	parentAchievements->opciones.push_back(opcionEnableAchievements);

	//Incluimos un indicador para habilitar el modo hardcore
	OpcionBool *opcionHardcore = new OpcionBool(LanguageManager::instance()->get("menu.achievement.hardcore"), &refConfig->configMain[cfg::hardcoreRA].getBoolRef());
	opcionHardcore->callback = &GestorMenus::changeHardcoreMode;
	parentAchievements->opciones.push_back(opcionHardcore);

	OpcionTxtAndValue *opcionRAUser = new OpcionTxtAndValue(LanguageManager::instance()->get("menu.achievement.user"), cfg::raUser);
	opcionRAUser->callback = &GestorMenus::changeRAUser;
	opcionRAUser->context = refConfig;
	opcionRAUser->editable = true;
	parentAchievements->opciones.push_back(opcionRAUser);

	string password = refConfig->configMain[cfg::raPass].getStringRef();
	OpcionTxtAndValue *opcionRAPassword = new OpcionTxtAndValue(LanguageManager::instance()->get("menu.achievement.password"), cfg::raPass);
	opcionRAPassword->callback = &GestorMenus::changeRAPassword;
	opcionRAPassword->context = refConfig;
	opcionRAPassword->editable = true;
	opcionRAPassword->setPassword(true);
	parentAchievements->opciones.push_back(opcionRAPassword);

	Menu* menuSearchGamesGuide = new Menu(LanguageManager::instance()->get("menu.guides.title"), face_h_big * 2, this->getW() - marginX, menuRaiz);

	//Este menu no cuelga de ningun lado, pero ponemos partidas guardadas como padre
	menuAskSavestates = new Menu(LanguageManager::instance()->get("menu.guides.search.title"), menuSavestates);
        
    todosLosMenus.push_back(menuRaiz);
    todosLosMenus.push_back(menuVideo);
	todosLosMenus.push_back(menuEmulation);
	todosLosMenus.push_back(menuEntrada);
	todosLosMenus.push_back(menuCoreOptions);
	todosLosMenus.push_back(menuCheats);
	todosLosMenus.push_back(menuSavestates);
	todosLosMenus.push_back(menuAskSavestates);
	todosLosMenus.push_back(menuScrapper);
	todosLosMenus.push_back(parentAchievements);
	todosLosMenus.push_back(menuSearchGamesGuide);

	//Poblar menu emulacion
	//--------Menu de sincronizacion de video---------
    std::vector<std::string> syncvals;
	for (int i=0; i < TOTAL_VIDEO_SYNC; i++){
		syncOptionsStrings[i] = LanguageManager::instance()->get("menu.sync.sync" + Constant::TipoToStr(i));
		syncvals.push_back(syncOptionsStrings[i]);
	}
	menuEmulation->opciones.push_back(new OpcionLista(LanguageManager::instance()->get("menu.options.sync"), syncvals, &refConfig->configMain[cfg::syncMode].getIntRef()));
	
	//--------Opcion de fastforward---------
	menuEmulation->opciones.push_back(new OpcionInt(LanguageManager::instance()->get("menu.options.fastforward"), 
		&refConfig->configMain[cfg::fastForwardMult].getIntRef(), "%.1fx", 10));
	
	//--------Opcion de fps---------
	menuEmulation->opciones.push_back(new OpcionBool(LanguageManager::instance()->get("menu.options.fps"), &refConfig->configMain[cfg::showFps].getBoolRef()));
	
	//--------Opcion de mostrar emuladores vacios---------
	menuEmulation->opciones.push_back(new OpcionBool(LanguageManager::instance()->get("menu.video.showempty"), &refConfig->configMain[cfg::showEmptyEmulators].getBoolRef()));

	//--------Menu de gestion de discos---------
	menuDisks = new Menu(LanguageManager::instance()->get("menu.disk.control"), menuEmulation);
	poblarMenuDiscos(BOOT_WITH_DISK);
	menuEmulation->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.disk.control"), menuDisks));
	
	//--------Opcion para cambiar la ruta del directorio de roms---------
	OpcionTxtAndValue *opcionChangeRomPath = new OpcionTxtAndValue(LanguageManager::instance()->get("menu.options.romdir"), cfg::roms_path);
	opcionChangeRomPath->callback = &GestorMenus::changeRomPath;
	opcionChangeRomPath->context = refConfig;
	opcionChangeRomPath->editable = true;
	menuEmulation->opciones.push_back(opcionChangeRomPath);

    //Poblar Menu Video
	//Relacion de aspecto
	std::vector<std::string> aspectRates;
	for (int i=0; i < TOTAL_VIDEO_RATIO; i++){
		aspectRatioStrings[i] = LanguageManager::instance()->get("menu.aspect.aspect" + Constant::TipoToStr(i));
		aspectRates.push_back(aspectRatioStrings[i]);
	}
	menuVideo->opciones.push_back(new OpcionLista(LanguageManager::instance()->get("menu.options.aspect"), aspectRates, &refConfig->configMain[cfg::aspectRatio].getIntRef()));

	//Escalado de video
    std::vector<std::string> filtros;

#if defined(_XBOX) || defined(SALVIA_GPU_VIDEO)
	for (int i=0; i < TOTAL_SHADERS; i++){
		videoShaderStrings[i] = LanguageManager::instance()->get("menu.video.shader" + Constant::TipoToStr(i));
		filtros.push_back(videoShaderStrings[i]);
	}
	menuVideo->opciones.push_back(new OpcionLista(LanguageManager::instance()->get("menu.options.scale"), filtros, &refConfig->configMain[cfg::shaderMode].getIntRef()));
#else
	for (int i=0; i < TOTAL_VIDEO_SCALE; i++){
		videoScaleStrings[i] = LanguageManager::instance()->get("menu.scale.scale" + Constant::TipoToStr(i));
		filtros.push_back(videoScaleStrings[i]);
	}
	menuVideo->opciones.push_back(new OpcionLista(LanguageManager::instance()->get("menu.options.scale"), filtros, &refConfig->configMain[cfg::scaleMode].getIntRef()));
#endif
	
	menuVideo->opciones.push_back(new OpcionBool(LanguageManager::instance()->get("menu.options.integerscale"), &refConfig->configMain[cfg::integerScale].getBoolRef()));
	
	std::vector<std::string> scaleInt;
	for (int i=0; i < TOTAL_INT_SCALE; i++){
		videoIntScaleStrings[i] = LanguageManager::instance()->get("menu.options.integerscale.type" + Constant::TipoToStr(i));
		scaleInt.push_back(videoIntScaleStrings[i]);
	}
	menuVideo->opciones.push_back(new OpcionLista(LanguageManager::instance()->get("menu.options.integerscale.type"), scaleInt, &refConfig->configMain[cfg::scaleIntMode].getIntRef()));


	//Animacion del fondo de pantalla del menu
	std::vector<std::string> bgMenu;
	for (int i=0; i < BG_MAX; i++){
		bgMenu.push_back(LanguageManager::instance()->get("menu.background.anim" + Constant::TipoToStr(i)));
	}

	#ifndef _XBOX
	//En xbox siempre mostramos pantalla completa
	menuVideo->opciones.push_back(new OpcionBool(LanguageManager::instance()->get("menu.video.fullscreen"), &refConfig->configMain[cfg::fullscreen].getBoolRef()));
	#endif
	OpcionLista *listaBkg = new OpcionLista(LanguageManager::instance()->get("menu.background.anim.title"), bgMenu, &refConfig->configMain[cfg::animBG].getIntRef());
	listaBkg->callback = &GestorMenus::selectBackground;
	menuVideo->opciones.push_back(listaBkg);

	//Resolucion de pantalla (se aplica al REINICIAR). Entrada 0 = "Auto"
	//(Xbox: XGetVideoMode del dashboard, capado a 720p; Windows: default 1280x720).
	std::vector<std::string> resList;
	resList.push_back("Auto");
	for (int i=0; i < TOTAL_SCREEN_RES; i++){
		resList.push_back(Constant::string_format("%dx%d", g_screenResolutions[i].w, g_screenResolutions[i].h));
	}

	{
		const int rw = refConfig->configMain[cfg::resolution_width].valueInt;
		const int rh = refConfig->configMain[cfg::resolution_height].valueInt;
		if (rw > 0 && rh > 0){
			const std::string cur = Constant::string_format("%dx%d", rw, rh);
			int f = -1;
			for (int i=1; i < (int)resList.size(); i++){ if (resList[i] == cur){ f = i; break; } }
			//valor del fichero fuera de la lista (p.ej. Windows editado a mano) -> anadir para no perderlo
			if (f < 0){ resList.push_back(cur); f = (int)resList.size() - 1; }
			refConfig->configMain[cfg::resolutionIndex].setPropValue(f);
		}
	}
	OpcionLista *listaRes = new OpcionLista(LanguageManager::instance()->get("menu.video.resolution"), resList, &refConfig->configMain[cfg::resolutionIndex].getIntRef());
	listaRes->callback = &GestorMenus::selectResolution;
	listaRes->context  = refConfig;
	menuVideo->opciones.push_back(listaRes);

	//--------Menu de overscan---------
	menuOverscan = new Menu(LanguageManager::instance()->get("menu.video.overscan"), menuVideo);
	poblarMenuOverscan(menuOverscan);
	menuVideo->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.video.overscan"), menuOverscan));
	
	menuAssignRetro = new Menu(LanguageManager::instance()->get("menu.options.paddassign"), menuEntrada);
	menuAssignFrontend = new Menu(LanguageManager::instance()->get("menu.options.frontassign"), menuEntrada);
	Menu* menuHotkeys = new Menu(LanguageManager::instance()->get("menu.options.hotkeys"), menuEntrada);
	Menu* menuRapidFire = new Menu(LanguageManager::instance()->get("menu.options.rapidfire"), menuEntrada);
	todosLosMenus.push_back(menuAssignRetro);
	todosLosMenus.push_back(menuAssignFrontend);
	todosLosMenus.push_back(menuHotkeys);
	todosLosMenus.push_back(menuRapidFire);

	menuEntrada->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.options.paddassign"), menuAssignRetro));
	menuEntrada->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.options.frontassign"), menuAssignFrontend));
	menuEntrada->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.options.hotkeys"), menuHotkeys));
	menuEntrada->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.options.rapidfire"), menuRapidFire));
	menuEntrada->opciones.push_back(new OpcionExec<Joystick>(LanguageManager::instance()->get("menu.options.saveassign"), &GestorMenus::guardarJoysticks, joystick, this));

	//Traducciones para las teclas
	std::size_t num_elementos = sizeof(FRONTEND_BTN_VAL) / sizeof(FRONTEND_BTN_VAL[0]);
	for (std::size_t i=0; i < num_elementos; i++){
		FRONTEND_BTN_TXT[i] = LanguageManager::instance()->get("menu.controls.frontkey" + Constant::TipoToStr(i));
	}
	for (int i=0; i < HK_MAX; i++){
		HOTKEYS_STR[i] = LanguageManager::instance()->get("menu.controls.hotkey" + Constant::TipoToStr(i));
	}

	num_elementos = sizeof(configurablePortButtons) / sizeof(configurablePortButtons[0]);
	for (std::size_t i=0; i < num_elementos; i++){
		configurablePortButtonsStr[i] = LanguageManager::instance()->get("menu.controls.retrobtn" + Constant::TipoToStr(i));
	}

	num_elementos = sizeof(configurablePortHats) / sizeof(configurablePortHats[0]);
	for (std::size_t i=0; i < num_elementos; i++){
		configurablePortHatsStr[i] = LanguageManager::instance()->get("menu.controls.retropad" + Constant::TipoToStr(i));
	}

	for (int i=0; i < KEY_JOY_MAX; i++){
		TipoKeyStr[i] = LanguageManager::instance()->get("menu.inputs.key" + Constant::TipoToStr(i));
	}

	for (int controlId = 0; controlId < MAX_PLAYERS; controlId++){
		std::string controlStr = LanguageManager::instance()->get("menu.options.portcontrols") 
			+ std::string(" ") + Constant::TipoToStr(controlId + 1) + " " +
			joystick->inputs.names[controlId];

		Menu* menuControlesPuerto = new Menu(controlStr , menuAssignRetro);
		addControlerOptions(menuControlesPuerto, controlId, joystick, refConfig);
		addControlerButtons(menuControlesPuerto, controlId, joystick);
		menuAssignRetro->opciones.push_back(new OpcionSubMenu(controlStr, menuControlesPuerto));
		todosLosMenus.push_back(menuControlesPuerto);
	}

	menuAssignRetro->opciones.push_back(new OpcionExec<Joystick>(LanguageManager::instance()->get("menu.options.savecoreassign"), &GestorMenus::guardarCoreJoysticks, joystick, this));
	menuAssignRetro->opciones.push_back(new OpcionExec<Joystick>(LanguageManager::instance()->get("menu.options.savegameassign"), &GestorMenus::guardarGameJoysticks, joystick, this));


	//Poblar menu hotkeys
	poblarMenuHotkeys(menuHotkeys, joystick);
	//Poblar menu disparo rapido
	poblarMenuRapidFire(menuRapidFire, joystick);
	//Menu de teclas para el frontend
	poblarMenuAssignFrontend(menuAssignFrontend, joystick);
	//Menu del scrapper que rellena los idiomas y lenguas
	poblarMenuScrapper(refConfig, menuScrapper);

	//Poblar menu ask
	std::vector<std::string> askOptions;
	for (int i=0; i < MAX_ASK; i++){
		ACTION_ASK_STR[i] = LanguageManager::instance()->get("menu.ask.action" + Constant::TipoToStr(i));
		askOptions.push_back(ACTION_ASK_STR[i]);
	}
	menuAskSavestates->opciones.push_back(new OpcionLista(LanguageManager::instance()->get("menu.options.askTitle"), askOptions, &askNumOptions));

	//Preferencias del core bajo el menu de emulacion
	Menu *menuCoreOverrides = new Menu(LanguageManager::instance()->get("menu.core.overrides"), menuEmulation);
	poblarMenuCoreOverrides(menuCoreOverrides, refConfig);
	menuEmulation->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.core.overrides"), menuCoreOverrides));

	// Poblar Menu Principal
    menuRaiz->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.main.video"), menuVideo, ico_video));
	menuRaiz->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.main.emulation"), menuEmulation, ico_settings));
	menuRaiz->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.main.input"), menuEntrada, ico_remap));
	menuRaiz->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.main.core.options"), menuCoreOptions, ico_settings_core));
	menuRaiz->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.main.cheats"), menuCheats, ico_cheats));
	menuRaiz->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.main.saves"), menuSavestates, ico_savestates));
	menuRaiz->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.main.scrapper"), menuScrapper, ico_scrapper));
	menuRaiz->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.achievement.title"), parentAchievements, ico_achievements));

	OpcionSubMenu *submenuSearchGuides = new OpcionSubMenu(LanguageManager::instance()->get("menu.guides.search.title"), menuSearchGamesGuide, ico_help);
	submenuSearchGuides->callback = &GestorMenus::gameSearchAction;
	submenuSearchGuides->context = this;
	menuGuides = new Menu(LanguageManager::instance()->get("menu.guides.title"), face_h_big * 2, this->getW() - marginX, menuSearchGamesGuide);
	menuGuideText = new Menu(LanguageManager::instance()->get("menu.guides.content"), menuGuides);
	menuRaiz->opciones.push_back(submenuSearchGuides);

	menuRaiz->opciones.push_back(new OpcionExec<CfgLoader>(LanguageManager::instance()->get("menu.main.saveconfig"), &GestorMenus::guardarMainConfig, refConfig, ico_saving, this));
	menuRaiz->opciones.push_back(new OpcionExec<CONFIG_STATUS>(LanguageManager::instance()->get("menu.main.return"), &GestorMenus::volverEmulacion, &status, ico_return, this));
	menuRaiz->opciones.push_back(new OpcionExec<CONFIG_STATUS>(LanguageManager::instance()->get("menu.main.exit"), &GestorMenus::salirEmulacion, &status, ico_shutdown, this));
	
	// Establecer estado inicial
    menuActual = menuRaiz;
	resetIndexPos();
}

void GestorMenus::checkMultipleSystemCore(CfgLoader *refConfig, Menu *menu, int coreIdx){
	if (coreIdx < 0 || (std::size_t)coreIdx >= refConfig->emulators.size()) return;
	
	dirutil dir;
	const int nCores = refConfig->emulators[coreIdx]->config.cores.size();
	const std::string emulatorTxt = LanguageManager::instance()->get("menu.core.overrides.emulator");

	if (nCores > 1){
		std::vector<std::string> coreNames;
		for (int core=0; core < nCores; core++){
			coreNames.push_back(dir.getFileNameNoExt(refConfig->emulators[coreIdx]->config.cores[core]));
		}
		OpcionLista *listaCores = new OpcionLista(emulatorTxt, coreNames, &refConfig->emulators[coreIdx]->config.execIdx);
		listaCores->callback = &GestorMenus::setDefaultEmu;
		listaCores->context = refConfig->emulators[coreIdx].get();
		menu->opciones.push_back(listaCores);
	}
}

std::string GestorMenus::gameSearchAction(void* inst){
	GestorMenus* gesMenu = static_cast<GestorMenus*>(inst);
	GameFaqsMenu* faq = &gesMenu->gameFaqsMenu;

	dirutil dir;
	std::vector<Opcion*> *list = &gesMenu->menuActual->opciones;
	LOG_DEBUG("submenu elementos: %d", list->size());
	std::string filename = Constant::limpiarNombreJuego(dir.getFileNameNoExt(faq->gameName));
	
	if (filename.empty()){
		return LanguageManager::instance()->get("msg.guides.notfound");
	}

	LOG_DEBUG("Searching faq for: %s", filename.c_str());
	std::size_t foundGames = faq->gameFaqs.searchGame(filename);

	if (foundGames > 0){
		list->clear();
		const std::vector<GameResult> *games = faq->gameFaqs.getGames();
		for (std::size_t i=0; i < foundGames; i++){
			LOG_DEBUG("game found: %s", games->at(i).name.c_str());
			OpcionGameFaq *opcion = new OpcionGameFaq(games->at(i));
			opcion->callback = &GestorMenus::gameGuidesSearchAction;
			opcion->context = gesMenu;
			list->push_back(opcion);
		}
	} else {
		return LanguageManager::instance()->get("msg.guides.notfound");
	}
	return "";
}

/**
*
*/
std::string GestorMenus::gameGuidesSearchAction(void* inst, void *value){
	GestorMenus* gesMenu = static_cast<GestorMenus*>(inst);
	const std::vector<Opcion*> *listActual = &gesMenu->menuActual->opciones;
	const std::size_t actualPos = gesMenu->menuActual->seleccionado;
	std::vector<Opcion*> *list = &gesMenu->menuGuides->opciones;
	GameFaqsMenu* faq = &gesMenu->gameFaqsMenu;

	if (actualPos < listActual->size() && listActual->at(actualPos)->tipo == OPC_FAQ_SEARCH){
		std::size_t foundGuides = faq->gameFaqs.findGuides(gesMenu->menuActual->seleccionado);
		if (foundGuides > 0){
			const std::vector<GuidesResult> *guides = faq->gameFaqs.getGuides();
			int index = -1;
			list->clear();

			for (std::size_t i=0; i < foundGuides; i++){
				LOG_DEBUG("guide found: %s", guides->at(i).name.c_str());
				OpcionFaq *opcion = new OpcionFaq(guides->at(i));

				if (guides->at(i).categ_id != index){
					index = guides->at(i).categ_id;
					const std::string categ = faq->gameFaqs.getCategories()->at(index);
					OpcionFaq *opcionCateg = new OpcionFaq(categ);
					list->push_back(opcionCateg);
				}

				opcion->callback = &GestorMenus::gameGuideAction;
				opcion->context = inst;
				opcion->valor.guidePos = i;
				list->push_back(opcion);
			}
		}
	}
	gesMenu->menuActual = gesMenu->menuGuides;
	return "";
}

/**
*
*/
std::string GestorMenus::gameGuideAction(void* inst, void *value){
	GestorMenus* gesMenu = static_cast<GestorMenus*>(inst);
	const std::vector<Opcion*> *listActual = &gesMenu->menuActual->opciones;
	const std::size_t actualPos = gesMenu->menuActual->seleccionado;
	std::vector<Opcion*> *list = &gesMenu->menuGuideText->opciones;
	GameFaqsMenu* faq = &gesMenu->gameFaqsMenu;

	LOG_DEBUG("Downloading guide in txt");
	if (actualPos < listActual->size() && listActual->at(actualPos)->tipo == OPC_FAQ_SELECT){
		OpcionFaq *opcion = (OpcionFaq *)listActual->at(actualPos);
		GuideContent guideContent;
        faq->gameFaqs.getGuideText(opcion->valor.guidePos, guideContent);
		list->clear();

		if (guideContent.type == GUIDE_TXT){
			LOG_DEBUG("text guide length: %d", guideContent.text.length());
			std::stringstream stream(guideContent.text);
			std::string linea;	
			// El bucle lee el flujo linea a linea hasta que se termina el texto
			while (std::getline(stream, linea)) {
				list->push_back(new OpcionTxt(linea));
			}
		} else if (guideContent.type == GUIDE_IMG){
			LOG_DEBUG("Image url: %s", guideContent.url.c_str());
			OpcionImage *opcionImage = new OpcionImage(guideContent.url);
			list->push_back(opcionImage);
		}
	}
	gesMenu->menuActual = gesMenu->menuGuideText;
	return "";
}

void GestorMenus::iniciarFiltros(GameDataFields& gameDataFieldsFilter){
		//Menu para los filtros
	menuGameFilter = new Menu(LanguageManager::instance()->get("menu.filter.title"));
	menuGameFilter->opciones.push_back(new OpcionListaRef(LanguageManager::instance()->get("menu.filter.system"), &gameDataFieldsFilter.systems, &gameDataFieldsFilter.posSystem));
	menuGameFilter->opciones.push_back(new OpcionListaRef(LanguageManager::instance()->get("menu.filter.manufacturer"), &gameDataFieldsFilter.manufacturers, &gameDataFieldsFilter.posManufacturer));
	menuGameFilter->opciones.push_back(new OpcionListaRef(LanguageManager::instance()->get("menu.filter.year"), &gameDataFieldsFilter.years, &gameDataFieldsFilter.posYear));
	menuGameFilter->opciones.push_back(new OpcionBool(LanguageManager::instance()->get("menu.filter.parents"), &gameDataFieldsFilter.onlyParents));
}

void GestorMenus::poblarMenuCoreOverrides(Menu *menu, CfgLoader *refConfig){
	const std::string aspectTxt = LanguageManager::instance()->get("menu.options.aspect");
	const std::string scaleOrShaderTxt = LanguageManager::instance()->get("menu.options.scale");
	const std::string intScaleTxt = LanguageManager::instance()->get("menu.options.integerscale");
	const std::string intScaleTypeTxt = LanguageManager::instance()->get("menu.options.integerscale.type");
	const std::string showDirTxt = LanguageManager::instance()->get("menu.options.showdir");
	const std::string recursiveFilesTxt = LanguageManager::instance()->get("menu.options.listrecursive");
	const std::string autoOverrideTxt = LanguageManager::instance()->get("menu.core.overrides.auto");

	//Aspect ratios texts
	std::vector<std::string> aspectRates;
	aspectRates.push_back(autoOverrideTxt);
	for (int j=0; j < TOTAL_VIDEO_RATIO; j++){
		aspectRates.push_back(aspectRatioStrings[j]);
	}

	//Video shader or scaler
    std::vector<std::string> filtros;
	filtros.push_back(autoOverrideTxt);

	#if defined(_XBOX) || defined(SALVIA_GPU_VIDEO)
		for (int i=0; i < TOTAL_SHADERS; i++){
			filtros.push_back(videoShaderStrings[i]);
		}
	#else
		for (int i=0; i < TOTAL_VIDEO_SCALE; i++){
			filtros.push_back(videoScaleStrings[i]);
		}
	#endif

    //Enable integer scaling
    std::vector<std::string> enableScaleInt; 
	enableScaleInt.push_back(autoOverrideTxt);
	enableScaleInt.push_back(LanguageManager::instance()->get("menu.core.overrides.disabled"));
	enableScaleInt.push_back(LanguageManager::instance()->get("menu.core.overrides.enabled"));
	
	//Integer scale mode
	std::vector<std::string> scaleInt;
	scaleInt.push_back(autoOverrideTxt);
	for (int i=0; i < TOTAL_INT_SCALE; i++){
		scaleInt.push_back(videoIntScaleStrings[i]);
	}

	for (std::size_t i=0; i < refConfig->emulators.size() - 1; i++){
		Menu *menuCore = new Menu(refConfig->emulators[i]->config.name, menu);
		menu->opciones.push_back(new OpcionSubMenu(refConfig->emulators[i]->config.name, menuCore));
		//Check if the system has more than one core available: eg: Megadrive has picodrive and genesis-plus-gx
		checkMultipleSystemCore(refConfig, menuCore, i);
		//Aspect ratio list
		menuCore->opciones.push_back(new OpcionLista(aspectTxt, aspectRates, &refConfig->emulators[i]->config.aspectRatio));
		//Shaders list
		#if defined(_XBOX) || defined(SALVIA_GPU_VIDEO)
		menuCore->opciones.push_back(new OpcionLista(scaleOrShaderTxt, filtros, &refConfig->emulators[i]->config.shaderMode));
		#else
		menuCore->opciones.push_back(new OpcionLista(scaleOrShaderTxt, filtros, &refConfig->emulators[i]->config.scaleMode));
		#endif
		//Integer scale enabler button
		menuCore->opciones.push_back(new OpcionLista(intScaleTxt, enableScaleInt, &refConfig->emulators[i]->config.integerScale));
		//Integer scale type
		menuCore->opciones.push_back(new OpcionLista(intScaleTypeTxt, scaleInt, &refConfig->emulators[i]->config.scaleIntMode));
		//Scan subfolders
		menuCore->opciones.push_back(new OpcionBool(recursiveFilesTxt, &refConfig->emulators[i]->config.menu_directory_recursive));
		//Show directories (no effect if menu_directory_recursive enabled)
		menuCore->opciones.push_back(new OpcionBool(showDirTxt, &refConfig->emulators[i]->config.menu_show_directories));
		
		//Button to save configuration of the selected core
		t_save_override *overr = new t_save_override(i, refConfig);
		menuCore->opciones.push_back(new OpcionExec<t_save_override>(LanguageManager::instance()->get("menu.main.saveconfig"), &GestorMenus::guardarCoreOverridesConfig, overr, this));
	}
}

void GestorMenus::poblarMenuOverscan(Menu *menu){
	const std::string format = LanguageManager::instance()->get("menu.video.overscan.format");

	OpcionInt* overscan_x = new OpcionInt(LanguageManager::instance()->get("menu.video.overscan.x"), 
		&CfgLoader::configMain[cfg::overscan_x].getIntRef(), format, 1);
	overscan_x->allowNegative = true;

	OpcionInt* overscan_y = new OpcionInt(LanguageManager::instance()->get("menu.video.overscan.y"), 
		&CfgLoader::configMain[cfg::overscan_y].getIntRef(), format, 1);
	overscan_y->allowNegative = true;
	
	menu->opciones.push_back(overscan_x);
	menu->opciones.push_back(overscan_y);
}

void GestorMenus::poblarMenuDiscos(int options){
	menuDisks->opciones.clear();
	
	OpcionTxtAndValue* nextCd = new OpcionTxtAndValue(LanguageManager::instance()->get("menu.disk.nextcd"), LanguageManager::instance()->get("menu.disk.nom3u"));
	nextCd->callback = &GestorMenus::cdromNextSelected;

	//Anyadimos la opcion de seleccion de discos
	cdromListMenu = new Menu(LanguageManager::instance()->get("menu.disk.selectcd"), menuDisks);
	OpcionSubMenu* diskSubmenu = new OpcionSubMenu(LanguageManager::instance()->get("menu.disk.selectcd"), cdromListMenu);
	diskSubmenu->callback = &GestorMenus::cdromListAction;
	diskSubmenu->context = cdromListMenu;
	menuDisks->opciones.push_back(diskSubmenu);
	menuDisks->opciones.push_back(nextCd);

	if (options == BOOT_NO_DISK){
		OpcionTxt* bootNoDisk = new OpcionTxt(LanguageManager::instance()->get("menu.disk.bootnocd"));
		bootNoDisk->callback = &GestorMenus::bootWithoutDisk;
		menuDisks->opciones.push_back(bootNoDisk);
	}
}

std::string GestorMenus::bootWithoutDisk(void* inst, void *value){
	launchBios();
	return "";
}

std::string GestorMenus::cdromFileSelected(void* inst, void *value) {
	std::string sendValue = *((std::string *)(value));
	FileProps* pFile = static_cast<FileProps*>(inst);
	std::string discFileToLoad = pFile->dir + Constant::getFileSep() + pFile->filename;
	LOG_DEBUG("cdromFileSelected: %s", discFileToLoad.c_str());
	swapToNewDisc(discFileToLoad);
	//delete pFile;
	return "";
}

std::string GestorMenus::cdromNextSelected(void* inst, void *value){
	if (disk_control.get_num_images && disk_control.get_num_images() > 1) {
		unsigned n   = disk_control.get_num_images();
		unsigned cur = disk_control.get_image_index ? disk_control.get_image_index() : 0;
		swapDisc((cur + 1) % n);
	} else {
		return LanguageManager::instance()->get("msg.cd.m3urequired");
	}
	return "";
}

std::string GestorMenus::cdromListAction(void* inst){
	Menu* listCdroms = static_cast<Menu*>(inst);
	if (listCdroms->opciones.size() <= 0){
		return LanguageManager::instance()->get("msg.cd.nofilestoselect");
	}
	return "";
}

void GestorMenus::poblarCdList(std::string ruta){
	dirutil dir;

	//Como pasamos un nuevo puntero, es mejor hacer el delete del new que se va a hacer mas adelante para cada elemento
	for (std::size_t i = 0; i < cdromListMenu->opciones.size(); ++i) {
		delete ((OpcionTxt *)cdromListMenu->opciones[i])->context;
	}
	cdromListMenu->opciones.clear();
	std::string ext = dir.getExtension(ruta);
	Constant::lowerCase(&ext);

	// Filtro del navegador de discos: extensiones que el CORE cargado declara
	// como validas (C64: d64/t64/prg/... ; PS1: cue/chd/...) UNIDAS a las de CD
	// (CD_FILTER), asi cualquier core con disk-control puede recargar sus propios
	// discos via swapToNewDisc sin perder el comportamiento previo de los cores
	// de CD. Formato "dotted" (".d64.t64...") que espera dirutil::foundFilter.
	std::string coreExts  = CfgLoader::configMain[cfg::libretro_core_extensions].valueStr;
	std::string extFilter = Constant::replaceAll(std::string(" ") + Constant::replaceAll(coreExts, "|", " "), " ", ".");
	extFilter            += Constant::replaceAll(std::string(CD_FILTER), " ", ".");

	if (extFilter.find(ext) == std::string::npos && ruta.find(BIOS_ONLY) == std::string::npos){
		//El fichero cargado no es cargable por el core actual: salimos
		return;
	}

	vector<unique_ptr<FileProps>> files;
	dir.listFiles(dir.getFolder(ruta).c_str(), files, extFilter, "", true, false);
	//Cada elemento del menu es un objeto FileProps con las propiedades del fichero seleccionado
	for (std::size_t i = 0; i < files.size(); ++i) {
		OpcionTxt *cdElem = new OpcionTxt(files[i]->filename);
		cdElem->callback = &GestorMenus::cdromFileSelected;
		// Creamos una copia nueva en memoria persistente
		FileProps* copia = new FileProps(*files[i]); 
		cdElem->context = copia; 
		cdromListMenu->opciones.push_back(cdElem);
	} 

	//Obtenemos el numero de cd's cargados y mostramos el numero seleccionado respecto al total
	unsigned n   = disk_control.get_num_images();
	unsigned cur = disk_control.get_image_index ? disk_control.get_image_index() : 0;
	if (menuDisks->opciones.size() > 1){
		OpcionTxtAndValue* nextCdBtn = static_cast<OpcionTxtAndValue*>(menuDisks->opciones[1]);
		if (nextCdBtn){
			if (n > 1){
				char buf[64];
				_snprintf(buf, sizeof(buf), LanguageManager::instance()->get("msg.cd.discnum").c_str(), cur + 1, n);
				buf[sizeof(buf) - 1] = '\0';
				nextCdBtn->valor = string(buf);
			} else {
				nextCdBtn->valor = LanguageManager::instance()->get("menu.disk.nom3u");
			}
		}
	}
}

void GestorMenus::loadAchievements() {
    // 1. Limpieza de opciones antiguas
    for (int i = 0; i < (int)menuAchievements->opciones.size(); i++) {
        if (menuAchievements->opciones[i]->tipo == OPC_ACHIEVEMENT) {
            OpcionAchievement* opt = (OpcionAchievement*)menuAchievements->opciones[i];
            delete opt;
        }
    }
    menuAchievements->opciones.clear();

    // 2. Carga desde la cola thread-safe
    Achievements& inst = *Achievements::instance();
    
    for (std::size_t i = 0; i < inst.achievements.size(); i++) {
        // Obtenemos una copia del logro sin hacer 'pop' (no se borra de la cola)
        AchievementState* currentAch = inst.achievements.get_at(i);
        // Creamos la opcion para el menu
        menuAchievements->opciones.push_back(new OpcionAchievement(*currentAch));
    }
    resetIndexPos();
}

std::string GestorMenus::selectBackground(void* inst, void *index, void *values) {
	//Cambio en vivo del fondo del menu (sin salir del menu). El estado retenido
	//se decide en setEmuStatus/arranque; aqui reflejamos el nuevo animBG.
	//Guard de rango: solo BG_HLSL..BG_NONE mapean a un shader de fondo; el resto
	//(tiles/imagen/none) apaga el fondo GPU.
	if (!index) return "";
	const int idx = *static_cast<int*>(index);
	HLSLBackground_setActive((idx >= BG_HLSL && idx < BG_NONE) ? (idx - BG_HLSL + 1) : 0);
	return "";
}

std::string GestorMenus::selectResolution(void* inst, void *index, void *values) {
	//Escribe la resolucion elegida en la config; se aplica al REINICIAR (el arranque
	//la lee). Parseamos la etiqueta seleccionada ("1280x720") para soportar tambien
	//entradas anadidas fuera de la lista estandar; "Auto" -> centinela 0/0.
	if (!inst || !index || !values) return "";
	CfgLoader* cfg = static_cast<CfgLoader*>(inst);
	const int idx = *static_cast<int*>(index);
	std::vector<std::string>* labels = static_cast<std::vector<std::string>*>(values);
	if (idx < 0 || idx >= (int)labels->size()) return "";
	int w = 0, h = 0; char sep = 0;
	std::istringstream iss(labels->at(idx));
	if ((iss >> w >> sep >> h) && sep == 'x'){   //"1280x720"
		cfg->setWidth(w);  cfg->setHeight(h);
	} else {
		cfg->setWidth(0);  cfg->setHeight(0);    //"Auto"
	}
	return "";
}

std::string GestorMenus::setDefaultEmu(void* inst, void *index, void *values) {
	if (!inst || !index || !values) return "";

	unsigned int sendIndex = *static_cast<int*>(index);
    std::vector<std::string>* sendValues = static_cast<std::vector<std::string>*>(values);
    ConfigEmu* cfgEmu = static_cast<ConfigEmu*>(inst);

	if (sendValues && sendIndex < sendValues->size()) {
		cfgEmu->executable = sendValues->at(sendIndex);
		#ifdef _XBOX
			cfgEmu->executable += ".xex";
		#else
			cfgEmu->executable += ".exe";
		#endif
		LOG_DEBUG("Seleccionando el emulador %s", cfgEmu->executable.c_str());
	}
	return "";
}

std::string GestorMenus::descargarLogros() { 
	Achievements::instance()->setShouldRefresh(true);
	Achievements::instance()->refresh_achievements_menu();
	loadAchievements();
	resetIndexPos();

	if (menuAchievements->opciones.size() > 0){
		BadgeDownloader::instance().start();
	}
	return ""; 
}

std::string GestorMenus::sDescargarLogros(void* inst) {
    return ((GestorMenus*)inst)->descargarLogros();
}

std::string GestorMenus::changeHardcoreMode(void* inst, void *value) {
	bool sendValue = *((bool *)(value));
	Achievements::instance()->setHardcoreMode(sendValue);
	if (sendValue){
		// Hardcore RetroAchievements: los cheats no estan permitidos. Desactivar todos
		// y quitarlos del core (applyToCore reaplica solo los habilitados = ninguno).
		// Solo si hay lista cargada (juego en marcha): asi no se llama a retro_cheat_reset
		// sin core cargado.
		std::vector<Cheat>& cheats = CheatManager::instance()->list();
		if (!cheats.empty()){
			for (std::size_t i = 0; i < cheats.size(); ++i)
				cheats[i].enabled = false;
			CheatManager::instance()->applyToCore();
		}
	}
	return "";
}

std::string GestorMenus::changeEnableAchievements(void* inst, void *value) {
	bool sendValue = *((bool *)(value));
	Achievements::instance()->logout();
	if (sendValue){
		const std::string user = CfgLoader::configMain[cfg::raUser].valueStr;
		const std::string pass = CfgLoader::configMain[cfg::raPass].valueStr;
		Achievements::instance()->login(user.c_str(), pass.c_str());
	}
	return "";
}

void GestorMenus::onRAUserText(const std::string& text, void* userData) {
	AskUserData* data = (AskUserData*)userData;
	if (!text.empty()) {
		*data->valorPtr = text;
		data->config->configMain[cfg::raUser].setPropValue(text);
		Achievements::instance()->logout();
		const std::string pass = data->config->configMain[cfg::raPass].valueStr;
		Achievements::instance()->login(text.c_str(), pass.c_str(), true);
	}
	delete data;
}

void GestorMenus::onRAPasswordText(const std::string& text, void* userData) {
	AskUserData* data = (AskUserData*)userData;
	if (!text.empty()) {
		*data->valorPtr = PASS_MASK;
		data->config->configMain[cfg::raPass].setPropValue(text);
		Achievements::instance()->logout();
		const std::string user = data->config->configMain[cfg::raUser].valueStr;
		Achievements::instance()->login(user.c_str(), text.c_str());
	} 
	delete data;
}

std::string GestorMenus::changeRAUser(void* inst, void *value) {
	AskUserData* data = new AskUserData();
	data->valorPtr = (std::string*)value;
	data->config = (CfgLoader*)inst;

	SOUtils::pedirTextoAsync("RetroAchievements", LanguageManager::instance()->get("menu.achievement.ask.user"),
			data->config->configMain[cfg::raUser].valueStr, &onRAUserText, data);
    return "";
}

std::string GestorMenus::changeRAPassword(void* inst, void *value) {
	AskUserData* data = new AskUserData();
	data->valorPtr = (std::string*)value;
	data->config = (CfgLoader*)inst;

	SOUtils::pedirTextoAsync("RetroAchievements", LanguageManager::instance()->get("menu.achievement.ask.password"),
			data->config->configMain[cfg::raPass].valueStr, &onRAPasswordText, data);
    return "";
}

std::string GestorMenus::changeRomPath(void* inst, void *value) {
	AskUserData* data = new AskUserData();
	data->valorPtr = (std::string*)value;
	data->config = (CfgLoader*)inst;

	SOUtils::pedirTextoAsync(LanguageManager::instance()->get("menu.options.romdir.asktitle"), 
							 LanguageManager::instance()->get("menu.options.romdir.askpath"),
							 data->config->configMain[cfg::roms_path].valueStr,
							 &onRomPath, data);
    return "";
}

void GestorMenus::onRomPath(const std::string& text, void* userData) {
	AskUserData* data = (AskUserData*)userData;
	if (!text.empty()) {
		*data->valorPtr = text;
		data->config->configMain[cfg::roms_path].setPropValue(text);
	}
	delete data;
}

void GestorMenus::onUserText(const std::string& text, void* userData) {
	AskUserData* data = (AskUserData*)userData;
	if (!text.empty()) {
		*data->valorPtr = data->isPassword ? PASS_MASK : text;
		data->config->configMain[data->cfgKey].setPropValue(text);
	}
	delete data;
}

std::string GestorMenus::changeScrapUser(void* inst, void *value) {
	AskUserData* data = new AskUserData();
	data->valorPtr = (std::string*)value;
	data->config = (CfgLoader*)inst;
	data->cfgKey = cfg::scrapUser;
	data->isPassword = false;

	SOUtils::pedirTextoAsync("Scrapper", LanguageManager::instance()->get("menu.scrap.ask.user"),
							 data->config->configMain[data->cfgKey].valueStr, &GestorMenus::onUserText, data);
    return "";
}

std::string GestorMenus::changeScrapPassword(void* inst, void *value) {
	AskUserData* data = new AskUserData();
	data->valorPtr = (std::string*)value;
	data->config = (CfgLoader*)inst;
	data->cfgKey = cfg::scrapPass;
	data->isPassword = true;

	SOUtils::pedirTextoAsync("Scrapper", LanguageManager::instance()->get("menu.scrap.ask.password"),
							 data->config->configMain[data->cfgKey].valueStr, &GestorMenus::onUserText, data);
    return "";
}

void GestorMenus::poblarMenuScrapper(CfgLoader *refConfig, Menu* menuScrapper){
	Menu* menuSistems = new Menu(LanguageManager::instance()->get("menu.scrap.systems"), menuScrapper);
	scrapSelection.resize(refConfig->emulators.size() - 1);
	std::vector<std::string> scrapGames;

	//Selection of the origin
	std::vector<std::string> scrapOrigin;
	for (int i=0; i < SC_MAX; i++){
		scrapOrigin.push_back(scrapOrigins[i]);
	}
	menuScrapper->opciones.push_back(new OpcionLista(LanguageManager::instance()->get("menu.scrap.from"), scrapOrigin, &refConfig->configMain[cfg::scrapOrigin].getIntRef()));

	//Selection of the systems to scan
	for (std::size_t i=0; i < refConfig->emulators.size() - 1; i++){
		scrapSelection[i].index = i;
		scrapSelection[i].name = refConfig->emulators[i]->config.name;
		scrapSelection[i].selected = false;
		menuSistems->opciones.push_back(new OpcionBool(scrapSelection[i].name, &scrapSelection[i].selected));
	}
	menuScrapper->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.scrap.systems"), menuSistems));

	//Selection of the artwork to download
	for (int i=0; i < TOTAL_SCRAP_GAMES; i++){
		scrapGames.push_back(LanguageManager::instance()->get("menu.scrap.games" + Constant::TipoToStr(i)));
	}
	menuScrapper->opciones.push_back(new OpcionLista(LanguageManager::instance()->get("menu.scrap.games"), scrapGames, &scrapGamesSelection));

	//Selection of other configuration
	Menu* menuScrapOptions = new Menu(LanguageManager::instance()->get("menu.scrap.other"), menuScrapper);
	if (refConfig->region.size() > 0){
		std::vector<std::string> regionDesc;
		std::string regCodeStr = refConfig->configMain[cfg::scrapRegion].valueStr;
		for (std::size_t i=0; i < refConfig->region.size(); i++){
			regionDesc.push_back(refConfig->region[i].desc);
			if (regCodeStr == refConfig->region[i].shortName){
				refConfig->idxRegion = i;
			}
		}
		menuScrapOptions->opciones.push_back(new OpcionLista(LanguageManager::instance()->get("menu.scrap.region"), regionDesc, &refConfig->idxRegion));
	}
		
	if (refConfig->idioma.size() > 0){
		std::vector<std::string> idiomaDesc;
		std::string idiomaCodeStr = refConfig->configMain[cfg::scrapLang].valueStr;
		for (std::size_t i=0; i < refConfig->idioma.size(); i++){
			idiomaDesc.push_back(refConfig->idioma[i].desc);
			if (idiomaCodeStr == refConfig->idioma[i].shortName){
				refConfig->idxIdioma = i;
			}
		}
		menuScrapOptions->opciones.push_back(new OpcionLista(LanguageManager::instance()->get("menu.scrap.lang"), idiomaDesc, &refConfig->idxIdioma));
	}

	OpcionTxtAndValue *opcionScrapUser = new OpcionTxtAndValue(LanguageManager::instance()->get("menu.scrap.user"), cfg::scrapUser);
	opcionScrapUser->callback = &GestorMenus::changeScrapUser;
	opcionScrapUser->context = refConfig;
	opcionScrapUser->editable = true;
	menuScrapOptions->opciones.push_back(opcionScrapUser);

	string password = refConfig->configMain[cfg::scrapPass].getStringRef();
	OpcionTxtAndValue *opcionScrapPassword = new OpcionTxtAndValue(LanguageManager::instance()->get("menu.scrap.password"), cfg::scrapPass);
	opcionScrapPassword->callback = &GestorMenus::changeScrapPassword;
	opcionScrapPassword->context = refConfig;
	opcionScrapPassword->editable = true;
	opcionScrapPassword->setPassword(true);
	menuScrapOptions->opciones.push_back(opcionScrapPassword);

	OpcionBool *opcionScrapToBin = new OpcionBool(LanguageManager::instance()->get("menu.scrap.packed"), &refConfig->configMain[cfg::packedImages].getBoolRef());
	menuScrapOptions->opciones.push_back(opcionScrapToBin);
	

	menuScrapper->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.scrap.other"), menuScrapOptions));
	menuScrapper->opciones.push_back(new OpcionExec<CONFIG_STATUS>(LanguageManager::instance()->get("menu.scrap.start"), &GestorMenus::startScrapping, &status, this));
}

/**
*
*/
void GestorMenus::addControlerOptions(Menu*& menu, int controlId, Joystick *joystick, CfgLoader *refConfig){
	menu->opciones.clear();
	if (controlId < MAX_PLAYERS){
		auto& controllerPad = joystick->g_ports[controlId];
		std::vector<std::string> gamepads;

		for (std::size_t i=0; i < controllerPad.available_types.size(); i++){
			if (i == 0 && controllerPad.current_device_id < 0){
				controllerPad.current_device_id = controllerPad.available_types.at(i).first;
				controllerPad.current_desc = controllerPad.available_types.at(i).second;
			}
			gamepads.push_back(controllerPad.available_types.at(i).second);
		}
		if (controllerPad.available_types.size() > 0){
			menuCoreOptions->opciones.push_back(new OpcionLista(controllerPad.current_desc, gamepads, &controllerPad.current_device_id));
		}
	}
	menu->opciones.push_back(new OpcionBool(LanguageManager::instance()->get("menu.controller.analogpad"), &joystick->inputs.axisAsPad[controlId]));
}

/**
*
*/
void GestorMenus::poblarJoystickTypes(Joystick *joystick){
	for (int i=0; i < (int)menuAssignRetro->opciones.size(); i++){
		LOG_DEBUG("%s", menuAssignRetro->opciones[i]->titulo.c_str());
		if (menuAssignRetro->opciones[i]->tipo == OPC_SUBMENU){
			Menu* submenuJoy = ((OpcionSubMenu*)menuAssignRetro->opciones[i])->destino;
			int numOpciones = submenuJoy->opciones.size();
			LOG_DEBUG("Menu with %d opciones", numOpciones);
			int nOpcion = 0;
			if (submenuJoy->opciones[nOpcion]->tipo == OPC_LISTA && numOpciones > 0){
				// 1. Liberar la memoria del objeto
				delete submenuJoy->opciones[nOpcion]; 
  				// 2. Eliminar el puntero del vector
				submenuJoy->opciones.erase(submenuJoy->opciones.begin() + nOpcion);
			}

			//Una vez liberadas las opciones de seleccion de joystick, las recreamos de nuevo
			auto available_types = joystick->g_ports[i].available_types;
			std::vector<std::string> joystickDesc;
			for (auto it = available_types.begin(); it != available_types.end(); ++it) {
				unsigned id = it->first;
				std::string name = it->second;
				joystickDesc.push_back(name);
				LOG_DEBUG("Player %d, JoyId: %u, Valor: %s\n", i, id, name.c_str());
			}

			OpcionLista *listaControllersTypes = new OpcionLista(LanguageManager::instance()->get("menu.controller.type"), joystickDesc, &joystick->inputs.joyTypeIdx[i]);
			listaControllersTypes->callback = &GestorMenus::setControllerType;
			listaControllersTypes->context = joystick;
			submenuJoy->opciones.insert(submenuJoy->opciones.begin(), listaControllersTypes);
		}
	}
}

/**
*
*/
std::string GestorMenus::setControllerType(void* inst, void *index, void *values) {
	if (!inst || !index || !values) return "";
    Joystick* joystick = static_cast<Joystick*>(inst);
	joystick->updateTypes();
	return "";
}

/**
*
*/
void GestorMenus::poblarCoreOptions(CfgLoader *refConfig){
    auto& paramsCore = refConfig->startupLibretroParams;
	auto& paramsGame = refConfig->gameSpecificLibretroParams;

	//Free the previous entries and the transient category submenus they pointed to.
	//OpcionSubMenu does not own its destino, so the Menu* must be deleted by hand;
	//its ~Menu cascades to the OpcionLista it holds.
	for (std::size_t i = 0; i < menuCoreOptions->opciones.size(); ++i)
		delete menuCoreOptions->opciones[i];
	menuCoreOptions->opciones.clear();
	for (std::size_t i = 0; i < coreOptionSubmenus.size(); ++i)
		delete coreOptionSubmenus[i];
	coreOptionSubmenus.clear();

	//Param independent options (kept at the menu root)
	menuCoreOptions->opciones.push_back(new OpcionExec<CfgLoader>(LanguageManager::instance()->get("menu.core.options.save"), &GestorMenus::guardarCoreConfig, refConfig, this));
	menuCoreOptions->opciones.push_back(new OpcionExec<CfgLoader>(LanguageManager::instance()->get("menu.core.options.restore"), &GestorMenus::restaurarCoreConfig, refConfig, this));
	menuCoreOptions->opciones.push_back(new OpcionTxtAndValue(LanguageManager::instance()->get("menu.core.options.version"), string(refConfig->configMain[cfg::libretro_core].valueStr) + " " + refConfig->configMain[cfg::libretro_core_version].valueStr));
	menuCoreOptions->opciones.push_back(new OpcionTxtAndValue(LanguageManager::instance()->get("menu.core.options.extensions"), refConfig->configMain[cfg::libretro_core_extensions].valueStr));

	//Main core options grouped by category (uncategorized ones stay at the root).
	//Game-specific options (e.g. FBNeo dip-switches) grouped the same way.
	addCoreOptionsByCategory(menuCoreOptions, refConfig->libretroCategories, paramsCore);
	addCoreOptionsByCategory(menuCoreOptions, refConfig->libretroCategories, paramsGame);
}

/**
* Puebla el submenu de cheats a partir de la lista cargada por CheatManager.
* Calca el patron de poblarCoreOptions.
*/
void GestorMenus::poblarCheats(CfgLoader *refConfig){
	menuCheats->opciones.clear();
	// Accion fija: recargar la lista desde el .cht en disco (todos desactivados).
	menuCheats->opciones.push_back(new OpcionExec<CfgLoader>(
		LanguageManager::instance()->get("menu.cheats.reload"), &GestorMenus::reloadCheats, refConfig, this));

	// Descargar de libretro-database. Siempre disponible (re-descarga / reemplaza el .cht).
	menuCheats->opciones.push_back(new OpcionExec<CfgLoader>(
		LanguageManager::instance()->get("menu.cheats.download"), &GestorMenus::descargarCheats, refConfig, this));

	std::vector<Cheat>& cheats = CheatManager::instance()->list();
	if (cheats.empty()){
		menuCheats->opciones.push_back(new OpcionTxt(LanguageManager::instance()->get("menu.cheats.none")));

		// Al cambiar de juego (o vaciar la lista) menuCheats se repuebla pero es persistente:
		// reiniciar la seleccion para no quedar con un indice obsoleto de la lista anterior.
		if (menuActual == menuCheats){
			resetIndexPos();
		} else {
			menuCheats->seleccionado = 0;
		}

		return;
	}

	// Justo despues del boton de descarga: nombre del .cht cargado. Solo con cheats
	// cargados (loadFromFile fija currentPath aunque el fichero no exista, asi que sin
	// esta guarda se mostraria un nombre para un .cht candidato inexistente).
	std::string chtName = CheatManager::instance()->displayFilename();
	if (!chtName.empty()){
		menuCheats->opciones.push_back(new OpcionTxt(chtName));
	}

	// El vector ya esta completo: los OpcionBool apuntan a &cheats[i].enabled, asi
	// que el vector no debe reasignarse despues de este punto.
	for (std::size_t i = 0; i < cheats.size(); ++i){
		OpcionBool* op = new OpcionBool(cheats[i].desc, &cheats[i].enabled);
		op->callback = &GestorMenus::sApplyCheats;   // re-aplica al alternar (A / izq-der)
		menuCheats->opciones.push_back(op);
	}

	// Al cambiar de juego (o vaciar la lista) menuCheats se repuebla pero es persistente:
	// reiniciar la seleccion para no quedar con un indice obsoleto de la lista anterior.
	if (menuActual == menuCheats){
		resetIndexPos();
	} else {
		menuCheats->seleccionado = 0;
	}

}

/**
* Recarga el .cht desde disco, repuebla el menu y reaplica al core.
*/
std::string GestorMenus::reloadCheats(CfgLoader *refConfig){
	CheatManager::instance()->reload();
	poblarCheats(refConfig);
	CheatManager::instance()->applyToCore();
	resetIndexPos();   // el numero de opciones pudo cambiar
	return LanguageManager::instance()->get("msg.cheats.reloaded");
}

/**
* Descarga bajo demanda el .cht del juego actual desde libretro-database y, si tiene
* exito, repuebla el submenu de cheats. Bloquea el overlay durante la descarga.
*/
std::string GestorMenus::descargarCheats(CfgLoader *refConfig){
	std::string path = downloadCheatWithProgress();
	if (path.empty())
		return LanguageManager::instance()->get("msg.cheats.download.none");

	CheatManager::instance()->loadFromFile(path);
	CheatManager::instance()->setSourceName(lastCheatSourceName());   // nombre real del .cht en GitHub
	poblarCheats(refConfig);
	resetIndexPos();
	return LanguageManager::instance()->get("msg.cheats.download.ok");
}

/**
* Callback de cada OpcionBool de cheat: reaplica toda la lista al alternar.
*/
std::string GestorMenus::sApplyCheats(void* inst, void* value){
	(void)inst;
	if (Achievements::instance()->isHardcoreMode()){
		// Hardcore RetroAchievements: no se permite activar cheats. Revertir el toggle
		// (el menu ya invirtio *value antes de llamar a este callback).
		if (value != NULL) *((bool*)value) = false;
		CheatManager::instance()->applyToCore();
		return "";
	}
	CheatManager::instance()->applyToCore();
	return "";
}

/**
*
*/
// Adds the visible options of `params` to `parent`, grouping them by libretro V2
// category: options without a category go straight into `parent` (root), and each
// category (in the order the core declared it in `categories`) becomes an
// OpcionSubMenu whose destino holds its options. Categories referenced by an option
// but not declared in `categories` are appended last (title = raw key) so no option
// is ever dropped. Persistence is untouched: every OpcionLista still binds to the
// &selected of its map entry, exactly as the old flat list did.
void GestorMenus::addCoreOptionsByCategory(Menu *parent,
		const std::vector<std::pair<std::string, std::string> > &categories,
		const std::map<std::string, std::unique_ptr<cfg::t_emu_props> > &params){

	if (params.empty())
		return;

	// Temp entry used to sort the visible options by description.
	struct TempElem {
		std::string key;
		std::string desc;
		std::string category;
	};

	std::vector<TempElem> sorter;
	for (auto it = params.begin(); it != params.end(); ++it) {
		if (it->second->isForThisGame) {
			TempElem e = { it->first, it->second->description, it->second->category };
			sorter.push_back(e);
		}
	}
	if (sorter.empty())
		return;

	std::sort(sorter.begin(), sorter.end(), [](const TempElem& a, const TempElem& b) {
		return Constant::compareNoCase(a.desc, b.desc);
	});

	// Helper to push one OpcionLista bound to the map entry's &selected.
	auto addOption = [&](Menu* dest, const std::string& key) {
		auto elem = params.find(key);
		if (elem != params.end()) {
			LOG_INFO("Key: %s, Selected: %d", elem->first.c_str(), elem->second->selected);
			const auto& displayItems = elem->second->labels.empty() ? elem->second->values : elem->second->labels;
			dest->opciones.push_back(new OpcionLista(elem->second->description, displayItems, &elem->second->selected));
		}
	};

	// 1. Uncategorized options -> straight into the root (sorted), old flat behavior.
	for (auto it = sorter.begin(); it != sorter.end(); ++it) {
		if (it->category.empty())
			addOption(parent, it->key);
	}

	// 2. Ordered list of categories that actually have visible options: first the
	//    ones the core declared (kept in declaration order), then any leftover key.
	std::vector<std::pair<std::string, std::string> > orderedCats; // key -> display desc
	for (auto c = categories.begin(); c != categories.end(); ++c) {
		for (auto it = sorter.begin(); it != sorter.end(); ++it) {
			if (it->category == c->first) { orderedCats.push_back(*c); break; }
		}
	}
	for (auto it = sorter.begin(); it != sorter.end(); ++it) {
		if (it->category.empty())
			continue;
		bool known = false;
		for (auto o = orderedCats.begin(); o != orderedCats.end(); ++o) {
			if (o->first == it->category) { known = true; break; }
		}
		if (!known)
			orderedCats.push_back(std::make_pair(it->category, it->category));
	}

	// 3. One submenu per category (in order), each with its options sorted by desc.
	for (auto c = orderedCats.begin(); c != orderedCats.end(); ++c) {
		Menu* catMenu = new Menu(c->second, parent);
		for (auto it = sorter.begin(); it != sorter.end(); ++it) {
			if (it->category == c->first)
				addOption(catMenu, it->key);
		}
		coreOptionSubmenus.push_back(catMenu);
		parent->opciones.push_back(new OpcionSubMenu(c->second, catMenu));
	}
}

/**
*
*/
void GestorMenus::poblarPartidasGuardadas(CfgLoader *refConfig, std::string rompath){
	this->lastImagePath = "";
	imageSavestate.closeImage();

	dirutil dir;
	const std::string statesDir = refConfig->configMain[cfg::libretro_state].valueStr + Constant::getFileSep() +
		refConfig->configMain[cfg::libretro_core].valueStr;
	const std::string keyToFind = STATE_EXT;
	std::string filterName = dir.getFileNameNoExt(rompath) + keyToFind;

	#ifdef _XBOX
	//Filtramos nombres largos o caracteres extranyos. sumamos un - para contemplar el tamanyo anyadido de los estados numerados
	filterName = dir.getFileNameNoExt(Constant::checkPath(statesDir + Constant::getFileSep() + filterName + "-"));
	#endif

	std::size_t pos = 0;
	std::string posSlot = "0";
	int found = -1;
	vector<unique_ptr<FileProps>> files;

	dir.listFiles(statesDir.c_str(), files, "", filterName, true, true);
	menuSavestates->opciones.clear();

	for (int i = 0; i < MAX_SAVESTATES; ++i) {
		OpcionSavestate *savestate = new OpcionSavestate(LanguageManager::instance()->get("menu.savestate.empty"));
		savestate->file.filename = filterName + Constant::intToString(i);
		savestate->file.dir = statesDir;
		savestate->status = &this->status;
		menuSavestates->opciones.push_back(savestate);
	}

	for (std::size_t i = 0; i < files.size(); ++i) {
		// Validaciones iniciales
		if (!files[i] || dir.getExtension(files[i]->filename) == STATE_IMG_EXT) continue;

		std::size_t pos = files[i]->filename.find(keyToFind);
		if (pos == std::string::npos) continue;

		LOG_DEBUG("File: %s", files[i]->filename.c_str());

		// Extraer indice de la ranura
		int iPosSlot = 0;
		if (pos + keyToFind.length() < files[i]->filename.length()){
			posSlot = files[i]->filename.substr(pos + keyToFind.length());
			iPosSlot = Constant::strToTipo<int>(posSlot);
		} else {
			posSlot = "0";
		}

		if (iPosSlot >= 0 && iPosSlot < (int)menuSavestates->opciones.size()) {
			// Usar un puntero temporal para legibilidad y evitar multiples casteos
			OpcionSavestate* opt = static_cast<OpcionSavestate*>(menuSavestates->opciones[iPosSlot]);
			// FileProps con copia segura
			opt->file = *files[i]; 
			// Para poder modificar el status si se pulsa este elemento y poder mostrar la emergente
			opt->status = &this->status; 
			// Titulo del elemento
			opt->titulo = LanguageManager::instance()->get("menu.savestate.slot") + " " + (posSlot.empty() ? "0" : posSlot);
		}
	}
}

void GestorMenus::poblarMenuHotkeys(Menu* menuHotkeys, Joystick *joystick){
	TipoKey type = KEY_JOY_BTN;
	t_joy_state *input = &joystick->inputs;

	for (int i=0; i < HK_MAX; i++){
		if (input->mapperHotkeys.getSdlHat(0, i) > -1){
			type = KEY_JOY_HAT;
		} else {
			type = KEY_JOY_BTN;
		}
		menuHotkeys->opciones.push_back(new OpcionKey(HOTKEYS_STR[i], input, &input->mapperHotkeys, 0, i, type, TipoKeyStr[type]));	
	}
}

void GestorMenus::poblarMenuAssignFrontend(Menu* menuAssign, Joystick *joystick){
	int num_port_buttons = sizeof(FRONTEND_BTN_VAL) / sizeof(FRONTEND_BTN_VAL[0]);
	TipoKey type = KEY_JOY_BTN;
	t_joy_state *input = &joystick->inputs;

	for (int i=0; i < num_port_buttons; i++){
		const std::string text = FRONTEND_BTN_TXT[i];
		const int fVal = FRONTEND_BTN_VAL[i];

		if (input->mapperFrontend.getSdlHat(0, fVal) > -1){
			type = KEY_JOY_HAT;
		} else {
			type = KEY_JOY_BTN;
		}

		menuAssign->opciones.push_back(new OpcionKey(text, input, &input->mapperFrontend, 0, fVal, type, TipoKeyStr[type]));	
	}
}

void GestorMenus::poblarMenuRapidFire(Menu* menuRapidFire, Joystick *joystick){
	t_joy_state *input = &joystick->inputs;

	// Selector de velocidad global. El indice se lee cada frame en retro_input_poll (sin callback).
	std::vector<std::string> rate;
	rate.push_back(LanguageManager::instance()->get("menu.rapidfire.rate.slow"));
	rate.push_back(LanguageManager::instance()->get("menu.rapidfire.rate.medium"));
	rate.push_back(LanguageManager::instance()->get("menu.rapidfire.rate.fast"));
	menuRapidFire->opciones.push_back(new OpcionLista(LanguageManager::instance()->get("menu.rapidfire.rate"), rate, &input->rapidFireRateIdx));

	// Un submenu por puerto (como menuAssignRetro), con un OpcionBool por boton.
	int num_port_buttons = sizeof(configurablePortButtons) / sizeof(configurablePortButtons[0]);
	for (int controlId = 0; controlId < MAX_PLAYERS; controlId++){
		std::string controlStr = LanguageManager::instance()->get("menu.options.portcontrols")
			+ std::string(" ") + Constant::TipoToStr(controlId + 1) + " " +
			input->names[controlId];

		Menu* menuPort = new Menu(controlStr, menuRapidFire);
		for (int sdlBtnIdx=0; sdlBtnIdx < num_port_buttons; sdlBtnIdx++){
			const std::string text = configurablePortButtonsStr[sdlBtnIdx];
			const int retroBtnValue = configurablePortButtons[sdlBtnIdx]; // id RETRO 0..15
			menuPort->opciones.push_back(new OpcionBool(text, &input->rapidFire[controlId][retroBtnValue]));
		}
		menuRapidFire->opciones.push_back(new OpcionSubMenu(controlStr, menuPort));
		todosLosMenus.push_back(menuPort);
	}
}

void GestorMenus::addControlerButtons(Menu*& menu, int controlId, Joystick *joystick){
	int num_port_buttons = sizeof(configurablePortButtons) / sizeof(configurablePortButtons[0]);
	int num_port_hats = sizeof(configurablePortHats) / sizeof(configurablePortHats[0]);
	
	t_joy_state *input = &joystick->inputs;
	//Adding the axis or pad elements
	for (int retroBtnIdx=0; retroBtnIdx < num_port_hats; retroBtnIdx++){
		const std::string text = configurablePortHatsStr[retroBtnIdx];
		const int retroBtnValue = configurablePortHats[retroBtnIdx];

		if (input->axisAsPad){
			menu->opciones.push_back(new OpcionKey(text, input, &input->mapperCore, controlId, retroBtnValue, KEY_JOY_AXIS, TipoKeyStr[KEY_JOY_AXIS]));
		} else {
			menu->opciones.push_back(new OpcionKey(text, input, &input->mapperCore, controlId, retroBtnValue, KEY_JOY_HAT, TipoKeyStr[KEY_JOY_HAT]));
		}
	}

	//Adding the buttons elements
	for (int sdlBtnIdx=0; sdlBtnIdx < num_port_buttons; sdlBtnIdx++){
		std::string text = configurablePortButtonsStr[sdlBtnIdx];
		const int retroBtnValue = configurablePortButtons[sdlBtnIdx];
		
		const int btnIdx = joystick->inputs.mapperCore.getSdlBtn(controlId, retroBtnValue);
		const int axisIdx = joystick->inputs.mapperCore.getSdlAxis(controlId, retroBtnValue);

		if (btnIdx > -1 || axisIdx == -1){
			menu->opciones.push_back(new OpcionKey(text, input, &input->mapperCore, controlId, retroBtnValue, KEY_JOY_BTN, TipoKeyStr[KEY_JOY_BTN]));	
		} else if (axisIdx > -1){
			menu->opciones.push_back(new OpcionKey(text, input, &input->mapperCore, controlId, retroBtnValue, KEY_JOY_AXIS, TipoKeyStr[KEY_JOY_AXIS]));	
		} 		
	}
}

int GestorMenus::findAxisPos(int retroDirection){
	int num_port_hats = sizeof(configurablePortHats) / sizeof(configurablePortHats[0]);
	for (int i=0; i < num_port_hats; i++){
		if (configurablePortHats[i] == retroDirection){
			return i;
		}
	}
	return -1;
}

// Logica para cambiar valores (Izquierda / Derecha)
void GestorMenus::cambiarValor(int dir) {
	if (status == POLLING_INPUTS || menuActual->opciones.size() == 0) return;

	if (status == ASK_SAVESTATES){
		Opcion* opt = menuAskSavestates->opciones[0];
		if (opt->tipo == OPC_LISTA) {
			OpcionLista* l = (OpcionLista*)opt;
			int num = (int)l->items.size();
			if (num > 0){
				*(l->indice) = (*(l->indice) + dir + num) % num;
			}
		}
		return;
	}

    Opcion* opt = menuActual->opciones[menuActual->seleccionado];
    if (opt->tipo == OPC_LISTA) {
        OpcionLista* l = (OpcionLista*)opt;
        int num = (int)l->items.size();
		if (num > 0){
			*(l->indice) = (*(l->indice) + dir + num) % num;
		}
		l->ejecutar();
    } else if (opt->tipo == OPC_BOOLEANA) {
        OpcionBool* b = (OpcionBool*)opt;
		LOG_DEBUG("cambiando de %s\n", *(b->valor) ? "S" : "N");
        *(b->valor) = !(*(b->valor));
		LOG_DEBUG("a %s\n",  *(b->valor) ? "S" : "N");
		b->ejecutar();
    } else if (opt->tipo == OPC_INT) {
		OpcionInt* i = (OpcionInt*)opt;
		if (!(dir < 0 && *(i->valor) == 0) || i->allowNegative){
			*(i->valor) = *(i->valor) + dir;
		}
		i->description = Constant::string_format(i->format, *(i->valor) / (float)i->divisor);
	} else if (opt->tipo == OPC_ACHIEVEMENT){
		for (int i=0; i < this->maxLines -1; i++){
			if (dir > 0){
				nextPos();
			} else {
				prevPos();
			}
		}
	}
}

void GestorMenus::resetAskPosition(){
	Opcion* e = menuAskSavestates->opciones[0];
	if (e->tipo == OPC_LISTA) {
		OpcionLista* l = (OpcionLista*)e;
		Opcion* opcionDelPadre = menuAskSavestates->padre->opciones[menuAskSavestates->padre->seleccionado];
		if (opcionDelPadre->tipo == OPC_SAVESTATE){
			*l->indice = 0;
		}
	}
}

// Logica para confirmar (Boton A)
std::string GestorMenus::confirmar(t_option_action *result) {
	if (status == POLLING_INPUTS || menuActual->opciones.size() == 0) return "";

	if (status == ASK_SAVESTATES){
		Opcion* e = menuAskSavestates->opciones[0];
		if (e->tipo == OPC_LISTA) {
			OpcionLista* l = (OpcionLista*)e;
			Opcion* opcionDelPadre = menuAskSavestates->padre->opciones[menuAskSavestates->padre->seleccionado];
			if (opcionDelPadre->tipo == OPC_SAVESTATE){
				OpcionSavestate *optSaves = static_cast<OpcionSavestate*>(opcionDelPadre);
				result->option = opcionDelPadre->tipo;
				result->action = *l->indice;
				result->indexSelected = menuAskSavestates->padre->seleccionado;

				std::string filepath = optSaves->file.dir + Constant::getFileSep() + optSaves->file.filename;
				if (!filepath.empty()) {
					result->elem = (void*)_strdup(filepath.c_str());
				} else {
					result->elem = NULL;
				}
				//Reseteamos la posicion del boton seleccionado
				*l->indice = 0;
			}
		}
		return e->ejecutar();
	}

	if ((std::size_t)menuActual->seleccionado >= menuActual->opciones.size()) return "";

    Opcion* opt = menuActual->opciones[menuActual->seleccionado];
	if (opt->tipo == OPC_SUBMENU || opt->tipo == OPC_FAQ_SEARCH || opt->tipo == OPC_FAQ_SELECT) {
		MenuStatus ms = {iniPos, endPos, curPos, maxLines, listSize, menuActual->seleccionado, menuActual};
		historyMenu.push_back(ms);
	}

    if (opt->tipo == OPC_SUBMENU) {
        menuActual = ((OpcionSubMenu*)opt)->destino;
		Opcion* e = (Opcion*)opt;
		std::string ret = e->ejecutar();
		resetIndexPos();
		return ret;
    } else if (opt->tipo == OPC_BOOLEANA) {
        cambiarValor(1);
	} else if (opt->tipo == OPC_KEY) {
		OpcionKey* k = (OpcionKey*)opt;
		k->changeAsked = true;
		k->lastTimeAsked = SDL_GetTicks();
		status = POLLING_INPUTS;
	} else if (opt->tipo == OPC_EXEC || opt->tipo == OPC_SAVESTATE || opt->tipo == OPC_SHOW_TXT || opt->tipo == OPC_SHOW_TXT_VAL 
		|| opt->tipo == OPC_FAQ_SEARCH || opt->tipo == OPC_FAQ_SELECT) {
		Opcion* e = (Opcion*)opt;
		std::string ret = e->ejecutar();

		if ((opt->tipo == OPC_FAQ_SEARCH && ((OpcionGameFaq*)opt)->callback != NULL) 
				|| (opt->tipo == OPC_FAQ_SELECT && ((OpcionFaq*)opt)->callback != NULL)){
			resetIndexPos();
		}
		return ret;
	} else if (opt->tipo == OPC_SHOW_IMG){
		const SDL_Rect fr = {0, 0, getW() - marginX, getH()};
		imageFaq.setTamAuto(!imageFaq.isTamAuto(), fr); 
	}

	
	
	return std::string("");
}

// Logica para volver (Boton B)
void GestorMenus::volver() {
	if (status == POLLING_INPUTS) return;

	if (status == ASK_SAVESTATES){
		resetAskPosition();
		LOG_DEBUG("Volviendo al menu de savestates");
		menuActual = menuAskSavestates->padre;
		status = NORMAL;
		//resetIndexPos();
		return;
	}

    if (menuActual->padre != NULL) {
        menuActual = menuActual->padre;
		if (!historyMenu.empty()){
			MenuStatus ms = historyMenu.back();
			if (menuActual == ms.menu){
				historyMenu.pop_back();
				this->iniPos = ms.iniPos;
				this->endPos = ms.endPos;
				this->curPos = ms.curPos;
				this->maxLines = ms.maxLines;
				this->listSize = ms.listSize;
				this->menuActual->seleccionado = ms.selectedMenuPos;
			}
		} else {
			resetIndexPos();
		}
    }
}

void GestorMenus::resetKeyElement(int sdlbtn, TipoKey tipoKey){
	//Buscamos en todos los elementos de menu y si hay alguna opcion con el mismo indice, lo ponemos a -1
	/*std::vector<Opcion*> optButtons = menuActual->opciones;
	for (int i=0; i < optButtons.size(); i++){
		if (optButtons[i]->tipo == OPC_KEY){
			OpcionKey* keyToReset = static_cast<OpcionKey*>(optButtons[i]);
			if (keyToReset->idx == sdlbtn && tipoKey == keyToReset->tipoKey){
				keyToReset->idx = -1;
			}
		}
	}*/
}

/**
*
*/
void GestorMenus::updateAxis(const SDL_Event &event){
	const int sdlAxisValue = event.jaxis.value;
	const int sdlAxis = event.jaxis.axis;

	if (status != POLLING_INPUTS) return;
	Opcion* opt = menuActual->opciones[menuActual->seleccionado];

	if (opt->tipo == OPC_KEY) {
		OpcionKey* k = static_cast<OpcionKey*>(opt);
		if (k && k->joyInputs) {
			if (abs(sdlAxisValue) > DEADZONE) {
				// 0 si es negativo (Izquierda/Arriba), 1 si es positivo (Derecha/Abajo)
				int isPositive = (sdlAxisValue > 0);
				int buttonIdx = (sdlAxis * 2) + isPositive;
				LOG_DEBUG("Eje: %d, Valor: %d -> Boton Virtual: %d", sdlAxis, sdlAxisValue, buttonIdx);
				k->tipoKey = KEY_JOY_AXIS;
				k->description = TipoKeyStr[k->tipoKey];
				resetKeyElement(buttonIdx, k->tipoKey);
				//k->joyInputs->setAxis(buttonIdx, k->btn);
				k->joyMapper->setAxisFromSdl(k->gamepadId, buttonIdx, k->btn);
				//k->idx = buttonIdx;
				k->changeAsked = false;
				k->lastTimeAsked = 0;
				status = NORMAL;
				k->joyInputs->clearAll();
				//La posicion de la opcion 0 es el elemento que anyadimos en addControlerOptions
				//en el orden de las inserciones en el vector.
				if (menuActual->opciones.size() > 0 && menuActual->opciones[0]->tipo == OPC_BOOLEANA) {	
					//Ponemos a true la opcion "Eje analagico como pad"
					OpcionBool* b = (OpcionBool*)menuActual->opciones[0];
					*(b->valor) = true;
				}
			} 
			//else {
				// CENTRO: Opcionalmente manejar el reposo aqui si es necesario
			//}
		}
	}
}

/**
*
*/
void GestorMenus::updateButton(const SDL_Event &event, TipoKey tipoKey){
	int joyNumber = -1; 
	int sdlbtn = -1;

	if (tipoKey == KEY_JOY_BTN){
		joyNumber = event.button.which;
		sdlbtn    = event.button.button;
	} else if (tipoKey == KEY_JOY_HAT){
		joyNumber = event.jhat.which;
		sdlbtn    = event.jhat.value;
	} else {
		return;
	}
	
	if (status != POLLING_INPUTS) return;
	Opcion* opt = menuActual->opciones[menuActual->seleccionado];

	if (opt->tipo == OPC_KEY) {
		OpcionKey* k = static_cast<OpcionKey*>(opt);

		//We avoid setting pads if the selected one is not correct
		if (joyNumber != k->gamepadId)
			return;

		if (k && k->joyInputs) {
			k->description = TipoKeyStr[tipoKey];
			k->tipoKey = tipoKey;

			if (k->tipoKey == KEY_JOY_BTN){
				k->joyMapper->setBtnFromSdl(k->gamepadId, sdlbtn, k->btn);
			} else if (k->tipoKey == KEY_JOY_HAT || k->tipoKey == KEY_JOY_AXIS){
				// Extraemos la direccion activa del Hat (limpiamos otros bits si fuera necesario)
				Uint8 sdlHatDir = (Uint8)(sdlbtn & (SDL_HAT_UP | SDL_HAT_DOWN | SDL_HAT_LEFT | SDL_HAT_RIGHT));
				k->joyMapper->setHatFromSdl(k->gamepadId, sdlbtn, k->btn);
			}
			
			//Reseteamos el estado
			k->joyInputs->clearAll();
			k->changeAsked = false;
			k->lastTimeAsked = 0;
			status = NORMAL;
		} else if (k && k->intRef) {
			int btnToSend = sdlbtn;
			if (tipoKey == KEY_JOY_HAT && (sdlbtn == SDL_HAT_DOWN || sdlbtn == SDL_HAT_UP || sdlbtn == SDL_HAT_LEFT || sdlbtn == SDL_HAT_RIGHT)){
				k->tipoKey = KEY_JOY_HAT;
				switch (sdlbtn){
					case SDL_HAT_DOWN:
						btnToSend = JOY_BUTTON_DOWN;
						break;
					case SDL_HAT_UP:
						btnToSend = JOY_BUTTON_UP;
						break;
					case SDL_HAT_LEFT:
						btnToSend = JOY_BUTTON_LEFT;
						break;
					case SDL_HAT_RIGHT:
						btnToSend = JOY_BUTTON_RIGHT;
						break;
				}
			} else {
				k->tipoKey = KEY_JOY_BTN;
			}
			
			k->joyInputs->clearAll();
			*k->intRef = btnToSend;
			k->description = TipoKeyStr[k->tipoKey];
			k->changeAsked = false;
			k->lastTimeAsked = 0;
			status = NORMAL;
		}
	}
}

Menu* GestorMenus::obtenerMenuActual() {
	return menuActual; 
}

void GestorMenus::draw(SDL_Surface *video_page){

	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	int face_h = menuActual->rowHeight;

	if (tmpTextOption == NULL || (tmpTextOption != NULL && tmpTextOption->format->BitsPerPixel != video_page->format->BitsPerPixel)){
		tmpTextOption = SDL_CreateRGBSurface(SDL_SWSURFACE, this->getW() - marginX, face_h, 
                                                       video_page->format->BitsPerPixel,
                                                       video_page->format->Rmask, 
                                                       video_page->format->Gmask, 
                                                       video_page->format->Bmask, 
                                                       video_page->format->Amask);
	}

	Fonts::drawTextCentTransparent(video_page, fontMenu, this->menuActual->titulo.c_str(), 0, face_h < marginY ? (marginY - face_h) / 2 : 0 , 
				true, false, Constant::colors[clWhite].sdlColor, 0);
	fastline(video_page, marginX, marginY - 1, video_page->w - marginX, marginY - 1, Constant::colors[clWhite].sdlColor);

    //To scroll one letter in one second. We use the face_h because the width of 
    //a letter is not fixed.
    const float pixelsScrollFps = std::max(ceil(face_h / (float)textFps), 1.0f);

	for (int i=this->iniPos; i < this->endPos && i < (int)this->menuActual->opciones.size(); i++){
        const auto& option = this->menuActual->opciones.at(i);
		std::string line;
		std::string value;

		if (option->tipo == OPC_SAVESTATE){
			drawSavestateWithImage(i, (OpcionSavestate *) option, video_page);
			continue;
		} else if (option->tipo == OPC_ACHIEVEMENT){
			drawAchievement(i, (OpcionAchievement *) option, video_page);
			continue;
		} else if (option->tipo == OPC_FAQ_SEARCH){
			drawFaqSearch(i, (OpcionGameFaq *) option, video_page);
			continue;
		} else if (option->tipo == OPC_FAQ_SELECT){
			drawFaqSelect(i, (OpcionFaq *) option, video_page);
			continue;
		} else if (option->tipo == OPC_SHOW_IMG){
			drawImage(i, (OpcionImage *) option, video_page);
			continue;
		} else if (option->tipo == OPC_BOOLEANA){
			line = option->titulo;// + " " + std::string(*((OpcionBool *)option)->valor ? "Y" : "N");
		} else if (option->tipo == OPC_LISTA){
			int indice = *((OpcionLista *)option)->indice;
			line = option->titulo;
			if ((unsigned int)indice < ((OpcionLista *)option)->items.size()){
				value = "< " + ((OpcionLista *)option)->items.at(indice) + " >";
			} else {
				value = "";
			}
		} else if (option->tipo == OPC_SUBMENU){
			line = option->titulo + " >";
		} else if (option->tipo == OPC_INT){
			line = option->titulo;// + " " + ((OpcionInt *)option)->description + Constant::intToString(*((OpcionInt *)option)->valor);
			value = ((OpcionInt *)option)->description;
		} else if (option->tipo == OPC_SHOW_TXT_VAL){
			line = option->titulo;
			value = ((OpcionTxtAndValue *)option)->valor;
		} else {
			line = option->titulo;
		}

		const int screenPos = i - this->iniPos;
        const int fontHeightRect = screenPos * face_h;
        const int lineBackground = -1;
        SDL_Color lineTextColor = i == this->curPos ? Constant::colors[clBlack].sdlColor : Constant::colors[clWhite].sdlColor;

        //Drawing a faded background selection rectangle
        if (i == this->curPos){
            int y = this->getY() + fontHeightRect;
            //Gaining some extra fps when the screen resolution is low
			SDL_Rect rectElem = {this->getX(), y, this->getW() - marginX, face_h};
            if (video_page->h >= 480){
				DrawRectAlpha(video_page, rectElem, Constant::colors[clBkgMenu].sdlColor, 190);
            } else {
                lineTextColor = Constant::colors[clWhite].sdlColor;
            }
			rect(video_page, rectElem.x - 1, rectElem.y - 1, rectElem.x + rectElem.w, rectElem.y + rectElem.h, Constant::colors[clBkgMenu].sdlColor);
        }
        
		int marginIco = 0;

		if (option->icon > -1 && option->icon < max_icons){
			marginIco = face_h;
			SDL_Rect dstRect = {this->getX(), this->getY() + fontHeightRect - Icons::getInstance().icon_w_add / 2, 0, 0};
			Icons::getInstance().drawIcon(video_page, &dstRect, option->icon);
		}

		Fonts::drawTextTransparent(video_page, fontMenu, line.c_str(), this->getX() + marginIco + Icons::getInstance().icon_w_add, 
                    this->getY() + fontHeightRect, lineTextColor, lineBackground);

		if (option->tipo == OPC_KEY && !((OpcionKey *)option)->description.empty()){
			drawKeys(i, (OpcionKey *)option, video_page);
		} else if (option->tipo == OPC_BOOLEANA){
			drawBooleanSwitch(i, (OpcionBool *)option, video_page);			
		} else if (!value.empty()){
			const int pixelDato = Fonts::getSize(fontMenu, value);
			Fonts::drawTextTransparent(video_page, fontMenu, value.c_str(), this->getX() + this->getW() - marginX - pixelDato - 1, 
                    this->getY() + fontHeightRect, lineTextColor, lineBackground);

		}
    }

	drawAskMenu(video_page);
	drawBordersMenuOverlay(video_page);

}

void GestorMenus::drawBordersMenuOverlay(SDL_Surface *video_page) {
    if (!isOverscanmenu()) return;

    const Uint32 bordersColor = Constant::colors[clWhite].color;
    const int thickness = 10;
    const int lineLen = (int)(video_page->h * 0.1);
    const int w = video_page->w;
    const int h = video_page->h;

    // Array de 8 rectangulos
    SDL_Rect rects[8] = {
        // Bordes Verticales Izquierda (Arriba / Abajo)
        {0, 0, (Uint16)thickness, (Uint16)lineLen},
        {0, (Sint16)(h - lineLen), (Uint16)thickness, (Uint16)lineLen},
        
        // Bordes Verticales Derecha (Arriba / Abajo)
        {(Sint16)(w - thickness), 0, (Uint16)thickness, (Uint16)lineLen},
        {(Sint16)(w - thickness), (Sint16)(h - lineLen), (Uint16)thickness, (Uint16)lineLen},
        
        // Bordes Horizontales Arriba (Izquierda / Derecha)
        {0, 0, (Uint16)lineLen, (Uint16)thickness},
        {(Sint16)(w - lineLen), 0, (Uint16)lineLen, (Uint16)thickness},
        
        // Bordes Horizontales Abajo (Izquierda / Derecha)
        {0, (Sint16)(h - thickness), (Uint16)lineLen, (Uint16)thickness},
        {(Sint16)(w - lineLen), (Sint16)(h - thickness), (Uint16)lineLen, (Uint16)thickness}
    };

    // En SDL 1.2 recorremos el array y llamamos a SDL_FillRect por cada uno
    for (int i = 0; i < 8; i++) {
        SDL_FillRect(video_page, &rects[i], bordersColor);
    }
}
void GestorMenus::drawKeys(int i, OpcionKey *opt, SDL_Surface *video_page){
	std::string str = "";
	const int screenPos = i - this->iniPos;
	const int fontHeightRect = screenPos * face_h_big;
	const SDL_Color& lineTextColor = i == this->curPos ? Constant::colors[clBlack].sdlColor : Constant::colors[clWhite].sdlColor;

	std::string titulo = this->menuActual->titulo;
	Constant::lowerCase(&titulo);
	//bool isGamepadXbox = titulo.find("xbox") != std::string::npos;
	bool isGamepadXbox = true;

	// 1. Manejo del temporizador (Early exit)
	Uint32 elapsed = SDL_GetTicks() - opt->lastTimeAsked;
	if (opt->changeAsked && elapsed < 4000) {
		str = LanguageManager::instance()->get("menu.inputs.waitkeypress") + Constant::intToString((5000 - elapsed) / 1000) + " s";
	} else {
		// 2. Obtener el ID de SDL segun el tipo de entrada
		int sdlIdBtn = -1;
		int sdlIdAxis = -1;

		if (opt->tipoKey == KEY_JOY_BTN){
			sdlIdBtn = opt->joyMapper->getSdlBtn(opt->gamepadId, opt->btn);
		} else if (opt->tipoKey == KEY_JOY_HAT || opt->tipoKey == KEY_JOY_AXIS){
			sdlIdBtn = opt->joyMapper->getSdlHat(opt->gamepadId, opt->btn);
			sdlIdAxis = opt->joyMapper->getSdlAxis(opt->gamepadId, opt->btn);
		}

		// 3. Procesar el resultado una sola vez
		if (sdlIdBtn > -1) {
			std::string keyStr = Constant::intToString(sdlIdBtn);
			if (opt->tipoKey == KEY_JOY_BTN && isGamepadXbox)
				keyStr = std::string(SDL_BTN_TO_XBOX[sdlIdBtn]);
			else if (isGamepadXbox)
				keyStr = std::string(SDL_HAT_TO_XBOX[sdlIdBtn]);
			str = (opt->tipoKey == KEY_JOY_BTN ? opt->description : TipoKeyStr[KEY_JOY_HAT]) + keyStr;
		} 

		if (sdlIdAxis > -1) {
			std::string axisStr = isGamepadXbox ? SDL_JOY_TO_XBOX[sdlIdAxis] : Constant::intToString(sdlIdAxis);
			str += (str.empty() ? "" : ", ") + TipoKeyStr[KEY_JOY_AXIS] + axisStr;
		} 

		// Resetear estado de edicion si estaba activo
		if (opt->changeAsked && (sdlIdBtn > -1 || sdlIdAxis > -1)) {
			opt->changeAsked = false;
			opt->lastTimeAsked = 0;
			status = NORMAL;
		}
		
		if (sdlIdBtn < 0 && sdlIdAxis < 0){
			str = "-";
		}
	}

	int pixelDato = Fonts::getSize(Fonts::FONTBIG, str);
	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	Fonts::drawTextTransparent(video_page, fontMenu, str.c_str(), this->getX() + this->getW() - marginX - pixelDato - 1, 
            this->getY() + fontHeightRect, lineTextColor, 0);
}

void GestorMenus::drawBooleanSwitch(int i, OpcionBool *opcion, SDL_Surface *video_page){
	// 1. Extraer el valor y definir dimensiones base
	bool enabled = *opcion->valor;
	const int screenPos = i - this->iniPos;
    const int fontHeightRect = screenPos * face_h_big;
	const int sw_h = face_h_big - 5;
	const int sw_w = sw_h * 2;
	const int sw_x = getX() + getW() - marginX - sw_w;
	const int sw_y = getY() + fontHeightRect + 2;

	// 2. Dibujar el fondo del switch
	SDL_Rect baseRect = { sw_x, sw_y, sw_w, sw_h };
	SDL_FillRect(video_page, &baseRect, enabled ? Constant::colors[clSwitchEnabled].color : Constant::colors[clSwitchDisabled].color);

	// 3. Calcular el thumb (boton interno) de forma relativa
	const int spacing = 4;
	const int size = sw_h - (spacing * 2);
	int thumbX = sw_x + (enabled ? (sw_w - size - spacing) : spacing);

	SDL_Rect thumbRect = { thumbX, sw_y + spacing, size, size };

	// 4. Dibujar el thumb segun el estado
	if (enabled) {
		SDL_FillRect(video_page, &thumbRect, Constant::colors[clBlack].color);
	} else {
		// Usando los campos de thumbRect directamente para evitar sumas manuales
		rect(video_page, thumbRect.x, thumbRect.y, thumbRect.x + size, thumbRect.y + size, Constant::colors[clBlack].sdlColor);
		rect(video_page, thumbRect.x + 1, thumbRect.y + 1, thumbRect.x + size - 1, thumbRect.y + size - 1, Constant::colors[clBlack].sdlColor);
	}
}

void GestorMenus::drawAskMenu(SDL_Surface *video_page) {
    if (status != ASK_SAVESTATES) return;

    TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
    const int ask_w = 520, ask_h = 200, btn_h = 30, btn_w = 150, marginTitle = 10;

    SDL_Rect thumbRect = { (this->w - ask_w) / 2, (this->h - ask_h) / 2, ask_w, ask_h };
    SDL_Rect titleRect = { thumbRect.x, thumbRect.y, thumbRect.w, 40 };

    // Dibujado de fondo y bordes (agrupado)
    SDL_FillRect(video_page, &thumbRect, Constant::colors[clAskBg].color);
    SDL_FillRect(video_page, &titleRect, Constant::colors[clAskTitle].color);
	rect(video_page, thumbRect.x, thumbRect.y, thumbRect.x + thumbRect.w, thumbRect.y + thumbRect.h, Constant::colors[clAskLine].sdlColor);
    rect(video_page, thumbRect.x - 1, thumbRect.y - 1, thumbRect.x + thumbRect.w + 1, thumbRect.y + thumbRect.h + 1, Constant::colors[clAskLine].sdlColor);

    Opcion* opt = menuAskSavestates->opciones[0];
    Fonts::drawTextTransparent(video_page, fontMenu, opt->titulo.c_str(), 
                                 titleRect.x + marginTitle, titleRect.y + (titleRect.h - face_h_big) / 2, Constant::colors[clAskText].sdlColor, 0);

    if (opt->tipo == OPC_LISTA) {
        OpcionLista* l = static_cast<OpcionLista*>(opt);
        if (l->items.size() <= 1) return;

        // Determinar si solo se permite guardar
        bool onlySave = false;
        Opcion* padre = menuAskSavestates->padre->opciones[menuAskSavestates->padre->seleccionado];
        if (padre->tipo == OPC_SAVESTATE) {
            onlySave = static_cast<OpcionSavestate*>(padre)->file.modificationTime.empty();
            if (onlySave) *(l->indice) = ASK_GUARDAR;
        }

        const int numItems = (int)l->items.size();
        const int freeSpace = (thumbRect.w - (numItems * btn_w) - 2 * marginTitle) / (numItems - 1);
        const int btnY = thumbRect.y + titleRect.h + (thumbRect.h - titleRect.h) / 2 - (btn_h / 2);

        for (int i = 0; i < numItems; i++) {
            // Simplificacion logica: Si es onlySave, saltar indices que no sean ASK_GUARDAR
            if (onlySave && i != ASK_GUARDAR) continue;

            bool isSelected = (i == *(l->indice));
            
            // Colores segun seleccion
            SDL_Color clText = isSelected ? Constant::colors[clAskTitle].sdlColor : Constant::colors[clAskText].sdlColor;
            int clBg = isSelected ? Constant::colors[clAskText].color : Constant::colors[clAskLine].color;

            SDL_Rect btnRect = { titleRect.x + 10 + ((btn_w + freeSpace) * i), btnY, btn_w, btn_h };

            // Dibujar boton
            SDL_FillRect(video_page, &btnRect, clBg);
            rect(video_page, btnRect.x, btnRect.y, btnRect.x + btnRect.w, btnRect.y + btnRect.h, clText);

            // Centrar texto en el boton
			int textW = Fonts::getSize(Fonts::FONTBIG, l->items[i]);
            Fonts::drawTextTransparent(video_page, fontMenu, l->items[i].c_str(), 
                                         btnRect.x + (btn_w - textW) / 2, btnRect.y + (btn_h - face_h_big) / 2, clText, 0);
        }
    }
}

/**
*
*/
void GestorMenus::drawSelectionBox(int i, SDL_Surface *video_page, SDL_Color& lineTextColor, int face_h){
	if (face_h == 0){
		face_h = menuActual->rowHeight;
	}

	const int screenPos = i - this->iniPos;
    const int fontHeightRect = screenPos * face_h;
	lineTextColor = i == this->curPos ? Constant::colors[clBlack].sdlColor : Constant::colors[clWhite].sdlColor;

	std::string rutaSelected;
	if (i == this->curPos){
        int y = this->getY() + fontHeightRect;
        //Gaining some extra fps when the screen resolution is low
		SDL_Rect rectElem = {this->getX(), y, menuActual->menuWidth, face_h};
        if (video_page->h >= 480){
			DrawRectAlpha(video_page, rectElem, Constant::colors[clBkgMenu].sdlColor, 190);
        } else {
            lineTextColor = Constant::colors[clWhite].sdlColor;
        }
		//Drawing the selection menu
		rect(video_page, rectElem.x - 1, rectElem.y - 1, rectElem.x + rectElem.w, rectElem.y + rectElem.h, Constant::colors[clBkgMenu].sdlColor);
    } 
}

/**
*
*/
void GestorMenus::drawFaqSearch(int i, OpcionGameFaq *opcion, SDL_Surface *video_page){
	const GameResult *gameResult = &opcion->valor;
	const int screenPos = i - this->iniPos;
	const int face_h = this->menuActual->rowHeight;
	const int fontHeightRect = screenPos * face_h;
	SDL_Color lineTextColor = i == this->curPos ? Constant::colors[clBlack].sdlColor : Constant::colors[clWhite].sdlColor;
	
	drawSelectionBox(i, video_page, lineTextColor, face_h);

	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	TTF_Font *fontSmall = Fonts::getFont(Fonts::FONTSMALL);

	Fonts::drawTextTransparent(video_page, fontMenu, gameResult->name.c_str(), this->getX() + marginX, 
            this->getY() + fontHeightRect, lineTextColor, 0);

	std::string details = gameResult->platform + " (" + gameResult->info + ")";

	Fonts::drawTextTransparent(video_page, fontSmall, details.c_str(), this->getX() + marginX, 
		this->getY() + fontHeightRect + face_h_big, lineTextColor, 0);
}

/**
*
*/
void GestorMenus::drawFaqSelect(int i, OpcionFaq *opcion, SDL_Surface *video_page){
	const GuidesResult *guideResult = &opcion->valor;
	const int screenPos = i - this->iniPos;
	const int face_h = this->menuActual->rowHeight;
	const int fontHeightRect = screenPos * face_h;
	SDL_Color lineTextColor = i == this->curPos ? Constant::colors[clBlack].sdlColor : Constant::colors[clWhite].sdlColor;
	
	drawSelectionBox(i, video_page, lineTextColor, face_h);

	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	TTF_Font *fontSmall = Fonts::getFont(Fonts::FONTSMALL);

	if (guideResult->categ_id >= 0){
		Fonts::drawTextTransparent(video_page, fontMenu, guideResult->name.c_str(), this->getX() + marginX, 
            this->getY() + fontHeightRect, lineTextColor, 0);

		std::string details = guideResult->author + " (" + (guideResult->platform.empty() ? "" : (guideResult->platform + ", ")) + guideResult->year + ")";

		Fonts::drawTextTransparent(video_page, fontSmall, details.c_str(), this->getX() + marginX, 
		this->getY() + fontHeightRect + face_h_big, lineTextColor, 0);
	} else {
		const std::string s = "----- " + guideResult->name + " -----";
		Fonts::drawTextTransparent(video_page, fontMenu, s.c_str(), this->getX() + marginX, 
            this->getY() + fontHeightRect + (face_h - face_h_big) / 2, i == this->curPos ? Constant::colors[clBlack].sdlColor : Constant::colors[clBlue].sdlColor);
	}
}

void GestorMenus::drawImage(int i, OpcionImage *opcion, SDL_Surface *video_page){
	if (!imageFaq.hasImage() || opcion->url != imageFaq.getFilepath()){
		gameFaqsMenu.gameFaqs.getImage(opcion->url, imageFaq, video_page->format);
		LOG_DEBUG("Downloading image: %s", opcion->url.c_str());
	} 
	imageFaq.printImage(video_page);
}

/**
*
*/
void GestorMenus::drawAchievement(int i, OpcionAchievement *opcion, SDL_Surface *video_page){
	SDL_Color lineTextColor = i == this->curPos ? Constant::colors[clBlack].sdlColor : Constant::colors[clWhite].sdlColor;
	drawSelectionBox(i, video_page, lineTextColor);

	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	TTF_Font *fontSmall = Fonts::getFont(Fonts::FONTSMALL);

	const int screenPos = i - this->iniPos;
	const int fontHeightRect = screenPos * menuActual->rowHeight;
	const int marginImg = 2;
	const int imgH = menuActual->rowHeight - 2*marginImg;

	const int position = this->getY() + fontHeightRect;
	if (opcion->achievement.isSection){
		const std::string s = "----- " + opcion->achievement.title + " -----";
		Fonts::drawTextTransparent(video_page, fontMenu, s.c_str(), 
			this->getX() + imgH + marginImg * 3, 
			position + menuActual->rowHeight / 2 - face_h_big / 2, 
			i == this->curPos ? Constant::colors[clBlack].sdlColor : Constant::colors[clBlue].sdlColor);
	} else {
		std::string firstLine = opcion->achievement.title;
		if (opcion->achievement.points > 0){
			firstLine += " (" + Constant::TipoToStr(opcion->achievement.points) + " point" + (opcion->achievement.points > 1 ? "s" : "") + ")";
		}
		//Drawing the first line of text on big font
		Fonts::drawTextTransparent(video_page, fontMenu, firstLine.c_str(), this->getX() + imgH + marginImg * 3, position, lineTextColor);
		//Drawing the second line of text on a smaller font
		Fonts::drawTextTransparent(video_page, fontSmall, opcion->achievement.description.c_str(), this->getX() + imgH + marginImg * 3, position + face_h_big, lineTextColor);
	}

    // Solo intentamos anyadir a la cola si NO tiene imagen Y NO se esta descargando ya
    if (opcion->achievement.badge == NULL && 
        !opcion->achievement.isDownloading && 
        !opcion->achievement.badgeUrl.empty()) {
        opcion->achievement.isDownloading = true; // Marcamos como "en proceso"
		BadgeDownloader::instance().add_to_deque(opcion->achievement, imgH, imgH);
		return;
    }

	// Elegimos el puntero pre-calculado
	SDL_Surface *surfaceToDraw = (opcion->achievement.locked) ? 
                                opcion->achievement.badgeLocked : 
                                opcion->achievement.badge;

	// Dibujar el badge si ya esta descargado
    if (surfaceToDraw != NULL && !opcion->achievement.isDownloading) {
        SDL_Rect dest;
        dest.x = this->getX() + marginImg;
        dest.y = position + marginImg;
		if (surfaceToDraw) {
			SDL_BlitSurface(surfaceToDraw, NULL, video_page, &dest);
		}
    }
}

/**
*
*/
void GestorMenus::drawSavestateWithImage(int i, OpcionSavestate *opcion, SDL_Surface *video_page){
	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	TTF_Font *fontSmall = Fonts::getFont(Fonts::FONTSMALL);
    const int screenPos = i - this->iniPos;
    const int fontHeightRect = screenPos * face_h_big;
    const int lineBackground = -1;
    SDL_Color lineTextColor = i == this->curPos ? Constant::colors[clBlack].sdlColor : Constant::colors[clWhite].sdlColor;
	const std::string line = opcion->titulo;
	std::string rutaSelected;

	drawSelectionBox(i, video_page, lineTextColor);

	if (i == this->curPos){
		rutaSelected = opcion->file.filename;
		//Drawing the modification date
		if (!opcion->file.modificationTime.empty()){
			//Drawing below the image
			Fonts::drawTextTransparent(video_page, fontSmall, std::string(LanguageManager::instance()->get("menu.savestate.latestsave") 
				+ opcion->file.modificationTime).c_str(), imageSavestate.getX(), 
                imageSavestate.getY() + imageSavestate.getH() + 2, Constant::colors[clWhite].sdlColor, 0);
		}
    } else {
		if (opcion->file.modificationTime.empty()){
			lineTextColor = Constant::colors[clMenuBars].sdlColor;
		}
	}

	//Drawing the text
    SDL_Rect txtRect = Fonts::drawTextTransparent(video_page, fontMenu, line.c_str(), this->getX(), 
                this->getY() + fontHeightRect, lineTextColor, lineBackground);
	
	//Drawing the image
	if (!rutaSelected.empty() && lastImagePath != rutaSelected){

		std::string rutaImg = opcion->file.dir + Constant::getFileSep() + opcion->file.filename + STATE_IMG_EXT;
		#ifdef _XBOX
		//Filtramos nombres largos o caracteres extranyos
		rutaImg = Constant::checkPath(rutaImg);
		#endif
		imageSavestate.loadImage(rutaImg);
		lastImagePath = opcion->file.filename;
	}

	//Drawing the date besides the text
	if (!opcion->file.modificationTime.empty()){
		Fonts::drawTextTransparent(video_page, fontSmall, opcion->file.modificationTime.c_str(), this->getX() + txtRect.w + 10, 
                this->getY() + fontHeightRect + face_h_small / 3, lineTextColor, lineBackground);
	}
	
	if (!rutaSelected.empty() && !opcion->file.modificationTime.empty()){
		imageSavestate.printImage(video_page);
	}
	//rect(video_page, imageSavestate.getX(), imageSavestate.getY(), imageSavestate.getX() + imageSavestate.getW(), imageSavestate.getY() + imageSavestate.getH(), white);
}

/**
*
*/
int GestorMenus::getScreenNumLines(){
	if (this->menuActual != NULL){
		int face_h = this->menuActual->rowHeight;
		return face_h != 0 ? (int)std::floor((double)getH() / face_h) : 0;
	}
	return 0;
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

// Logica de navegacion Arriba/Abajo
void GestorMenus::navegar(int dir) { // -1 o 1
    if (!menuActual || status == POLLING_INPUTS || status == ASK_SAVESTATES) return;

	if (dir > 0){
		if (this->curPos < this->listSize - 1){
			this->curPos++;
			menuActual->seleccionado = this->curPos;
			int posCursorInScreen = this->curPos - this->iniPos;
		
			if (posCursorInScreen > this->maxLines - 1){
				this->iniPos++;
				this->endPos++;
			}
			this->pixelShift = 0;
			this->lastSel = -1;
		}
	} else if (dir < 0){
		if (this->curPos > 0){
			this->curPos--;
			menuActual->seleccionado = this->curPos;
			if (this->curPos < this->iniPos && this->curPos >= 0){
				this->iniPos--;
				this->endPos--;
			}
			this->pixelShift = 0;
			this->lastSel = -1;
		}
	}

}

void GestorMenus::nextPos(){
    navegar(1);
}

void GestorMenus::prevPos(){
    navegar(-1);
}

void GestorMenus::nextPage(){
    for (int i=0; i < this->maxLines -1; i++){
        nextPos();
    }
}

void GestorMenus::prevPage(){
    for (int i=0; i < this->maxLines -1; i++){
        prevPos();
    }
}

void GestorMenus::volverMenuInicial(){
	status = NORMAL;
	menuActual = menuRaiz;
	resetIndexPos();
}


void GestorMenus::clearSelectedText(){
    if (imgText != NULL){
		SDL_FreeSurface(imgText);
        imgText = NULL;
    }
}