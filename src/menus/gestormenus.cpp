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
std::string configurablePortAnalogsStr[ANALOG_TARGETS];
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

// romPaths (global de salvia.h): ruta del juego cargado, para el guardado por-juego.
extern t_rom_paths romPaths;

// Guarda las opciones del core en un fichero JUNTO AL JUEGO (mismo nombre base
// + .opt). En la carga (launchGame -> CfgLoader::loadCoreParamsForGame) tiene
// prioridad sobre las opciones generales del core.
std::string GestorMenus::guardarCoreConfigGame(CfgLoader *refConfig){
	LOG_DEBUG("Guardando opciones del core para el juego actual");
	return refConfig->saveGameCoreParams(romPaths.rompath);
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


	//refConfig->deleteCoreParams();
	//dirutil dir;
	//const std::string corepath = refConfig->getCoreCfgPath();
	//if (dir.fileExists(corepath.c_str())){
	//	refConfig->appliedFileParmsCore = dir.getFileName(corepath);
	//} else {
		/** Aplicamos siempre las opciones por defecto. El usuario decide luego si quiere guardarlas como opciones del core
		 *  o como opciones del juego. Por eso mostramos un mensaje de opciones restauradas por defecto
		 */
		refConfig->appliedFileParmsCore = LanguageManager::instance()->get("menu.core.options.msg.default");
	//}
	refConfig->deleteGameParams(romPaths.rompath);

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

/* LanguageManager::get() devuelve "[la.clave]" cuando la clave no esta en el
 * .ini, que en el menu se ve feo. Para las etiquetas del stick derecho, que son
 * claves nuevas, caemos al texto en ingles hasta que esten traducidas. */
static std::string trOrDefault(const std::string &key, const char *fallback) {
	std::string s = LanguageManager::instance()->get(key);
	if (s.empty() || s[0] == '[')
		return std::string(fallback);
	return s;
}

// Inicializa la estructura de menus
void GestorMenus::inicializar(CfgLoader *refConfig, Joystick *joystick) {
	/* Stick izquierdo: claves propias, no las genericas menu.controls.*, que las
	 * usa tambien la cruceta (SDL_HAT_TO_XBOX) mas abajo. Compartiendolas, un eje
	 * y un hat se mostraban con el mismo texto y no habia forma de distinguirlos. */
	SDL_JOY_TO_XBOX[0] = trOrDefault("menu.controls.lstick.left",  "L-Left");
	SDL_JOY_TO_XBOX[1] = trOrDefault("menu.controls.lstick.right", "L-Right");
	SDL_JOY_TO_XBOX[2] = trOrDefault("menu.controls.lstick.up",    "L-Up");
	SDL_JOY_TO_XBOX[3] = trOrDefault("menu.controls.lstick.down",  "L-Down");

	/* Stick derecho. El indice depende de que eje SDL usa cada plataforma:
	 * en la 360 es el eje 2 (X) y el 3 (Y) -> 4..7; en Windows el eje 2 son los
	 * gatillos combinados y el stick derecho es el 3 (Y) y el 4 (X) -> 6..9.
	 * Ver salvia.cpp, RETRO_DEVICE_INDEX_ANALOG_RIGHT. */
#ifdef _XBOX
	SDL_JOY_TO_XBOX[4] = trOrDefault("menu.controls.rstick.left",  "R-Left");
	SDL_JOY_TO_XBOX[5] = trOrDefault("menu.controls.rstick.right", "R-Right");
	SDL_JOY_TO_XBOX[6] = trOrDefault("menu.controls.rstick.up",    "R-Up");
	SDL_JOY_TO_XBOX[7] = trOrDefault("menu.controls.rstick.down",  "R-Down");
#else
	SDL_JOY_TO_XBOX[6] = trOrDefault("menu.controls.rstick.up",    "R-Up");
	SDL_JOY_TO_XBOX[7] = trOrDefault("menu.controls.rstick.down",  "R-Down");
	SDL_JOY_TO_XBOX[8] = trOrDefault("menu.controls.rstick.left",  "R-Left");
	SDL_JOY_TO_XBOX[9] = trOrDefault("menu.controls.rstick.right", "R-Right");
#endif

	SDL_HAT_TO_XBOX[1] = LanguageManager::instance()->get("menu.controls.up");
	SDL_HAT_TO_XBOX[2] = LanguageManager::instance()->get("menu.controls.right");
	SDL_HAT_TO_XBOX[4] = LanguageManager::instance()->get("menu.controls.down");
	SDL_HAT_TO_XBOX[8] = LanguageManager::instance()->get("menu.controls.left");
	/* Diagonales: se componen de las cuatro de arriba para no pedir claves nuevas. */
	SDL_HAT_TO_XBOX[3]  = SDL_HAT_TO_XBOX[1] + "-" + SDL_HAT_TO_XBOX[2]; /* RIGHTUP   */
	SDL_HAT_TO_XBOX[6]  = SDL_HAT_TO_XBOX[4] + "-" + SDL_HAT_TO_XBOX[2]; /* RIGHTDOWN */
	SDL_HAT_TO_XBOX[9]  = SDL_HAT_TO_XBOX[1] + "-" + SDL_HAT_TO_XBOX[8]; /* LEFTUP    */
	SDL_HAT_TO_XBOX[12] = SDL_HAT_TO_XBOX[4] + "-" + SDL_HAT_TO_XBOX[8]; /* LEFTDOWN  */

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

	menuEntrada->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.options.paddassign"), menuAssignRetro, ico_remap));
	menuEntrada->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.options.frontassign"), menuAssignFrontend, ico_remap));
	menuEntrada->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.options.hotkeys"), menuHotkeys, ico_settings));
	menuEntrada->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.options.rapidfire"), menuRapidFire, ico_turbo));
	menuEntrada->opciones.push_back(new OpcionExec<Joystick>(LanguageManager::instance()->get("menu.options.saveassign"), &GestorMenus::guardarJoysticks, joystick, ico_saving, this));

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

	/* Direcciones analogicas. Claves nuevas: mientras no esten en los .ini se cae a
	 * un texto en ingles, en vez de mostrar "[menu.controls.retroanalog0]". */
	{
		static const char *analogFallback[ANALOG_TARGETS] = {
			"L-Analog Y- (Up)",   "L-Analog Y+ (Down)",
			"L-Analog X- (Left)", "L-Analog X+ (Right)",
			"R-Analog Y- (Up)",   "R-Analog Y+ (Down)",
			"R-Analog X- (Left)", "R-Analog X+ (Right)"
		};
		for (int i=0; i < ANALOG_TARGETS; i++){
			configurablePortAnalogsStr[i] = trOrDefault(
				"menu.controls.retroanalog" + Constant::TipoToStr(i), analogFallback[i]);
		}
	}

	for (int i=0; i < KEY_JOY_MAX; i++){
		const std::string keyName = "menu.inputs.key" + Constant::TipoToStr(i);
		/* KEY_JOY_ANALOG es nuevo y su clave puede no estar todavia; el resto se
		 * dejan como estaban para no enmascarar una clave que falte de verdad. */
		TipoKeyStr[i] = (i == KEY_JOY_ANALOG)
			? trOrDefault(keyName, "Analog: ")
			: LanguageManager::instance()->get(keyName);
	}

	for (int controlId = 0; controlId < MAX_PLAYERS; controlId++){
		std::string controlStr = LanguageManager::instance()->get("menu.options.portcontrols") 
			+ std::string(" ") + Constant::TipoToStr(controlId + 1) + " " +
			joystick->inputs.names[controlId];

		Menu* menuControlesPuerto = new Menu(controlStr , menuAssignRetro);
		addControlerOptions(menuControlesPuerto, controlId, joystick, refConfig);
		addControlerButtons(menuControlesPuerto, controlId, joystick);
		menuAssignRetro->opciones.push_back(new OpcionSubMenu(controlStr, menuControlesPuerto, ico_remap));
		todosLosMenus.push_back(menuControlesPuerto);
	}

	menuAssignRetro->opciones.push_back(new OpcionExec<Joystick>(LanguageManager::instance()->get("menu.options.savecoreassign"), &GestorMenus::guardarCoreJoysticks, joystick, ico_saving, this));
	menuAssignRetro->opciones.push_back(new OpcionExec<Joystick>(LanguageManager::instance()->get("menu.options.savegameassign"), &GestorMenus::guardarGameJoysticks, joystick, ico_saving, this));


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
	menuRaiz->opciones.push_back(new OpcionExec<CONFIG_STATUS>(LanguageManager::instance()->get("menu.main.return"), &GestorMenus::volverEmulacion, &status, ico_resume, this));
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
	const std::string syncTypeTxt = LanguageManager::instance()->get("menu.options.sync");
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

	//Synchronization mode
	std::vector<std::string> syncStrings;
	syncStrings.push_back(autoOverrideTxt);
	for (int i=0; i < TOTAL_VIDEO_SYNC; i++){
		syncStrings.push_back(syncOptionsStrings[i]);
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
		//Synchronization
		menuCore->opciones.push_back(new OpcionLista(syncTypeTxt, syncStrings, &refConfig->emulators[i]->config.syncMode));

		//Scan subfolders
		menuCore->opciones.push_back(new OpcionBool(recursiveFilesTxt, &refConfig->emulators[i]->config.menu_directory_recursive));
		//Show directories (no effect if menu_directory_recursive enabled)
		menuCore->opciones.push_back(new OpcionBool(showDirTxt, &refConfig->emulators[i]->config.menu_show_directories));
		
		//Button to save configuration of the selected core
		t_save_override *overr = new t_save_override(i, refConfig);
		menuCore->opciones.push_back(new OpcionExec<t_save_override>(LanguageManager::instance()->get("menu.main.saveconfig"), 
			&GestorMenus::guardarCoreOverridesConfig, overr, ico_saving, this));
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
	nextCd->context = nextCd;

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
		unsigned new_idx = (cur + 1) % n;

		if (swapDisc(new_idx)){
			OpcionTxtAndValue* option = static_cast<OpcionTxtAndValue*>(inst);
			char buf[64];
			_snprintf(buf, sizeof(buf), LanguageManager::instance()->get("msg.cd.discnum").c_str(), new_idx + 1, n);
			buf[sizeof(buf) - 1] = '\0';
			option->valor = buf;
		}
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
	menuScrapper->opciones.push_back(new OpcionExec<CONFIG_STATUS>(LanguageManager::instance()->get("menu.scrap.start"), &GestorMenus::startScrapping, &status, ico_resume, this));
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
	/* Aqui estaba la opcion "Eje analogico como pad" (axisAsPad). Ya no existe: era
	 * un interruptor global por jugador que solo sabia hacer "todos los ejes como
	 * cruceta". Ahora cada una de las ocho direcciones de stick lleva su propio
	 * destino, asi que el mismo efecto se consigue asignando las cuatro del stick
	 * izquierdo a las cuatro posiciones de la cruceta, y ademas se puede mezclar. */
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
	menuCoreOptions->opciones.push_back(new OpcionTxtAndDynValue(LanguageManager::instance()->get("menu.core.options.configfile"), refConfig->appliedFileParmsCore));
	menuCoreOptions->opciones.push_back(new OpcionExec<CfgLoader>(LanguageManager::instance()->get("menu.core.options.save"), &GestorMenus::guardarCoreConfig, refConfig, ico_saving, this));
	menuCoreOptions->opciones.push_back(new OpcionExec<CfgLoader>(LanguageManager::instance()->get("menu.core.options.savegame"), &GestorMenus::guardarCoreConfigGame, refConfig, ico_saving, this));
	menuCoreOptions->opciones.push_back(new OpcionExec<CfgLoader>(LanguageManager::instance()->get("menu.core.options.restore"), &GestorMenus::restaurarCoreConfig, refConfig, ico_reload, this));
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
		LanguageManager::instance()->get("menu.cheats.reload"), &GestorMenus::reloadCheats, refConfig, ico_reload, this));

	// Descargar de libretro-database. Siempre disponible (re-descarga / reemplaza el .cht).
	menuCheats->opciones.push_back(new OpcionExec<CfgLoader>(
		LanguageManager::instance()->get("menu.cheats.download"), &GestorMenus::descargarCheats, refConfig, ico_download, this));

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

	menuAssign->opciones.push_back(new OpcionBool(LanguageManager::instance()->get("menu.controller.analogpad"), &joystick->inputs.frontAxisAsPad));

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
			const int ico = ico_input_btn_d + sdlBtnIdx <= ico_input_r2 ? ico_input_btn_d + sdlBtnIdx : -1;

			menuPort->opciones.push_back(new OpcionBool(text, &input->rapidFire[controlId][retroBtnValue], ico));
		}
		menuRapidFire->opciones.push_back(new OpcionSubMenu(controlStr, menuPort, ico_remap));
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
		const int ico = ico_input_dpad_u + retroBtnIdx <= ico_input_dpad_r ? ico_input_dpad_u + retroBtnIdx : -1;

		/* Solo posiciones de cruceta. Antes esto elegia entre KEY_JOY_AXIS y
		 * KEY_JOY_HAT segun axisAsPad (con un bug: la condicion era sobre un array,
		 * o sea siempre cierta). Que el stick mueva la cruceta ya no se expresa
		 * aqui, sino asignando las direcciones del stick a posiciones de hat. */
		menu->opciones.push_back(new OpcionKey(text, input, &input->mapperCore, controlId, retroBtnValue, KEY_JOY_HAT, TipoKeyStr[KEY_JOY_HAT], ico));
	}

	//Adding the buttons elements
	for (int sdlBtnIdx=0; sdlBtnIdx < num_port_buttons; sdlBtnIdx++){
		std::string text = configurablePortButtonsStr[sdlBtnIdx];
		const int retroBtnValue = configurablePortButtons[sdlBtnIdx];
		
		const int btnIdx = joystick->inputs.mapperCore.getSdlBtn(controlId, retroBtnValue);
		const int axisIdx = joystick->inputs.mapperCore.getSdlAxis(controlId, retroBtnValue);
		const int ico = ico_input_btn_d + sdlBtnIdx <= ico_input_r2 ? ico_input_btn_d + sdlBtnIdx : -1;

		if (btnIdx > -1 || axisIdx == -1){
			menu->opciones.push_back(new OpcionKey(text, input, &input->mapperCore, controlId, retroBtnValue, KEY_JOY_BTN, TipoKeyStr[KEY_JOY_BTN], ico));
		} else if (axisIdx > -1){
			menu->opciones.push_back(new OpcionKey(text, input, &input->mapperCore, controlId, retroBtnValue, KEY_JOY_AXIS, TipoKeyStr[KEY_JOY_AXIS], ico));
		}
	}

	/* Deadzone de ESTE mando: cuanto hay que mover el stick para que una
	 * direccion convertida en boton o en cruceta cuente como pulsada. Va al
	 * final del menu del mando porque solo tiene sentido junto a las ocho
	 * direcciones de arriba, que son las que la usan.
	 *
	 * OpcionLista guarda el INDICE, asi que apunta directamente a
	 * mapperCore.deadzoneIdx[controlId]: no hace falta ni copia ni callback,
	 * el menu escribe donde lo lee joystick.cpp. */
	{
		std::vector<std::string> deadzoneItems;
		for (int i = 0; i < DEADZONE_STEPS; i++){
			/* El valor bruto (unidades de eje sobre 32767) no le dice nada a
			 * nadie, asi que se muestra como porcentaje del recorrido del
			 * stick, redondeado al entero mas cercano. Es SOLO presentacion:
			 * lo que se guarda sigue siendo el indice, asi que ni el .joy ni
			 * joystick.cpp se enteran. El maximo, 25000*100, cabe de sobra en
			 * un int. */
			const int pct = (deadzoneValues[i] * 100 + 16383) / 32767;
			deadzoneItems.push_back(Constant::intToString(pct) + "%");
		}

		/* Clave nueva: trOrDefault para que el menu se vea aunque el .ini del
		 * idioma todavia no la traiga, igual que se hizo con KEY_JOY_ANALOG. */
		menu->opciones.push_back(new OpcionLista(
			trOrDefault("menu.inputs.deadzone", "Deadzone: "),
			deadzoneItems,
			&input->mapperCore.deadzoneIdx[controlId]));
	}

	/* Direcciones de los sticks analogicos. A diferencia de las de arriba, el 'btn'
	 * no es un RETRO_DEVICE_ID_JOYPAD_* sino un JOY_AXIS1_* JOY_AXIS2_*, y el valor
	 * sale de mapperCore.analogDst. Sin icono: el calculo de 'ico' de los dos
	 * bucles anteriores asume tramos contiguos del enum y no hay iconos para esto. */
	for (int analogIdx = 0; analogIdx < ANALOG_TARGETS; analogIdx++){
		const std::string text = configurablePortAnalogsStr[analogIdx];
		const int retroBtnValue = configurablePortAnalogs[analogIdx];
		menu->opciones.push_back(new OpcionKey(text, input, &input->mapperCore, controlId,
			retroBtnValue, KEY_JOY_ANALOG, TipoKeyStr[KEY_JOY_ANALOG], ico_analog_u + (analogIdx % 4)));
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
		 || opt->tipo == OPC_SHOW_DYNTXT_VAL || opt->tipo == OPC_FAQ_SEARCH || opt->tipo == OPC_FAQ_SELECT) {
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
			/* Se loguean TODOS los eventos de eje, no solo los que pasan la deadzone:
			 * si un stick no responde al reasignar, lo primero que hay que saber es si
			 * sus eventos llegan siquiera y con que numero de eje. */
			LOG_DEBUG("updateAxis: mando %d, eje %d, valor %d (deadzone %d), destino btn %d",
				(int)event.jaxis.which, sdlAxis, sdlAxisValue, DEADZONE, k->btn);

			if (abs(sdlAxisValue) > DEADZONE) {
				// 0 si es negativo (Izquierda/Arriba), 1 si es positivo (Derecha/Abajo)
				int isPositive = (sdlAxisValue > 0);
				int buttonIdx = (sdlAxis * 2) + isPositive;
				/* Se decide por el DESTINO de la opcion, no por tipoKey, que se sobrescribe
				 * aqui mismo y no sirve para saber de que clase es. */
				const int analogSlot = t_joy_mapper::analogSlot(k->btn);
				LOG_DEBUG("updateAxis: -> direccion fisica %d, slot de la opcion %d", buttonIdx, analogSlot);

				if (analogSlot < 0) {
					/* Las cuatro de la cruceta solo admiten posiciones de hat, y los botones
					 * del core solo botones fisicos. Que un eje dispare algo ya no se asigna
					 * aqui: se hace desde la entrada del stick, diciendo en que se convierte. */
					LOG_DEBUG("updateAxis: la opcion (btn %d) no admite ejes, se ignora", k->btn);
					return;
				}

				/* Mover OTRO eje sobre una entrada de stick significa "esta direccion se
				 * comporta como aquella". De ahi salen el eje invertido y el intercambio
				 * entre sticks, sin codigo especial. */
				k->tipoKey = KEY_JOY_ANALOG;
				k->description = TipoKeyStr[k->tipoKey];
				k->joyMapper->setAnalogDst(k->gamepadId, analogSlot, buttonIdx);

				LOG_DEBUG("updateAxis: slot %d -> destino %d", analogSlot,
					k->joyMapper->getAnalogDst(k->gamepadId, analogSlot));

				k->changeAsked = false;
				k->lastTimeAsked = 0;
				status = NORMAL;
				k->joyInputs->clearAll();
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
			/* Entrada de stick: pulsar un boton o una direccion de la cruceta dice
			 * "esta direccion del stick se convierte en eso". Al cruzar la deadzone se
			 * encendera su bit simulado y a partir de ahi es indistinguible de una
			 * pulsacion real, asi que funciona con cualquier core. Se decide por el
			 * DESTINO, no por tipoKey, que se sobrescribe justo debajo. */
			const int analogSlot = t_joy_mapper::analogSlot(k->btn);
			if (analogSlot >= 0) {
				int dst;
				if (tipoKey == KEY_JOY_HAT) {
					/* Solo direcciones puras. Una diagonal (3, 6, 9 o 12) daria un
					 * mapeo mudo: getCoreHat busca la posicion que tenga asignada la
					 * direccion del core, que siempre es 1, 2, 4 u 8. Se ignora y se
					 * sigue esperando, en vez de guardar algo que no hace nada. */
					if (sdlbtn <= 0 || (sdlbtn & (sdlbtn - 1)) != 0) {
						LOG_DEBUG("updateButton: hat %d es diagonal, se ignora", sdlbtn);
						return;
					}
					dst = ANALOG_DST_HAT_BASE + sdlbtn;
				} else {
					dst = ANALOG_DST_BTN_BASE + sdlbtn;
				}
				k->tipoKey = KEY_JOY_ANALOG;
				k->description = TipoKeyStr[k->tipoKey];
				k->joyMapper->setAnalogDst(k->gamepadId, analogSlot, dst);
				LOG_DEBUG("updateButton: slot %d -> destino %d", analogSlot, dst);
				k->joyInputs->clearAll();
				k->changeAsked = false;
				k->lastTimeAsked = 0;
				status = NORMAL;
				return;
			}

			k->description = TipoKeyStr[tipoKey];
			k->tipoKey = tipoKey;

			if (k->tipoKey == KEY_JOY_BTN){
				k->joyMapper->setBtnFromSdl(k->gamepadId, sdlbtn, k->btn);
			} else if (k->tipoKey == KEY_JOY_HAT || k->tipoKey == KEY_JOY_AXIS){
				/* Solo direcciones puras, igual que en la rama de los sticks. Una
				 * diagonal (3, 6, 9 o 12) se guardaba tal cual y producia un mapeo
				 * MUDO: getCoreHat lee hats_state en el indice que tenga asignado el
				 * destino, y ese indice solo puede ser 1, 2, 4 u 8. Se ignora y se
				 * sigue esperando, en vez de guardar algo que no hace nada. */
				if (sdlbtn <= 0 || (sdlbtn & (sdlbtn - 1)) != 0) {
					LOG_DEBUG("updateButton: hat %d es diagonal, se ignora", sdlbtn);
					return;
				}
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

		SDL_Color valueColor = Constant::colors[clWhite].sdlColor;

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
			if (((OpcionTxtAndValue *)option)->callback == NULL){
				valueColor = Constant::colors[clHighligtOption].sdlColor;
			}
		} else if (option->tipo == OPC_SHOW_DYNTXT_VAL){
			line = option->titulo;
			value = *((OpcionTxtAndDynValue *)option)->valor;

			if (((OpcionTxtAndDynValue *)option)->callback == NULL){
				valueColor = Constant::colors[clHighligtOption].sdlColor;
			}
		} else {
			line = option->titulo;
		}

		const int screenPos = i - this->iniPos;
        const int fontHeightRect = screenPos * face_h;
        const int lineBackground = -1;
        SDL_Color lineTextColor = i == this->curPos ? Constant::colors[clBlack].sdlColor : Constant::colors[clWhite].sdlColor;
		valueColor = i == this->curPos ? Constant::colors[clBlack].sdlColor : valueColor;

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
                    this->getY() + fontHeightRect, valueColor);

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

		/* Las entradas de stick salen de otra tabla y con el destino codificado en un
		 * solo entero, asi que se resuelven aparte. Se mira el destino de la opcion y
		 * no tipoKey, por coherencia con updateAxis/updateButton. */
		const int analogSlot = t_joy_mapper::analogSlot(opt->btn);
		if (analogSlot >= 0) {
			const int dst = opt->joyMapper->getAnalogDst(opt->gamepadId, analogSlot);
			if (dst >= ANALOG_DST_HAT_BASE) {
				const int h = dst - ANALOG_DST_HAT_BASE;
				std::string keyStr = Constant::intToString(h);
				if (isGamepadXbox && h < SDL_HAT_TO_XBOX_SIZE && !SDL_HAT_TO_XBOX[h].empty())
					keyStr = SDL_HAT_TO_XBOX[h];
				str = TipoKeyStr[KEY_JOY_HAT] + keyStr;
			} else if (dst >= ANALOG_DST_BTN_BASE) {
				const int b = dst - ANALOG_DST_BTN_BASE;
				std::string keyStr = Constant::intToString(b);
				if (isGamepadXbox && b < SDL_BTN_TO_XBOX_SIZE && SDL_BTN_TO_XBOX[b][0] != '\0')
					keyStr = std::string(SDL_BTN_TO_XBOX[b]);
				str = TipoKeyStr[KEY_JOY_BTN] + keyStr;
			} else if (dst >= 0) {
				std::string axisStr = Constant::intToString(dst);
				if (isGamepadXbox && dst < SDL_JOY_TO_XBOX_SIZE && !SDL_JOY_TO_XBOX[dst].empty())
					axisStr = SDL_JOY_TO_XBOX[dst];
				str = TipoKeyStr[KEY_JOY_ANALOG] + axisStr;
			} else {
				str = "-";
			}

			// Resetear estado de edicion si estaba activo
			if (opt->changeAsked && dst > -1) {
				opt->changeAsked = false;
				opt->lastTimeAsked = 0;
				status = NORMAL;
			}
		}
		else if (opt->tipoKey == KEY_JOY_BTN){
			sdlIdBtn = opt->joyMapper->getSdlBtn(opt->gamepadId, opt->btn);
		} else if (opt->tipoKey == KEY_JOY_HAT){
			/* Solo el hat. Antes se leia tambien getSdlAxis y se mostraban los dos,
			 * porque las cuatro direcciones se podian asignar a cruceta Y a eje. Un
			 * .joy anterior a este cambio sigue trayendo ese 'axis=' (el viejo stick
			 * como cruceta), que ahora es codigo muerto -- nada escribe axis_state
			 * para esas direcciones dentro del juego -- pero se seguia pintando. */
			sdlIdBtn = opt->joyMapper->getSdlHat(opt->gamepadId, opt->btn);
		} else if (opt->tipoKey == KEY_JOY_AXIS){
			/* Se conserva para los gatillos: L2/R2 son botones del core alimentados
			 * por el eje 2 combinado de Windows, y esa via sigue viva. */
			sdlIdBtn = opt->joyMapper->getSdlHat(opt->gamepadId, opt->btn);
			sdlIdAxis = opt->joyMapper->getSdlAxis(opt->gamepadId, opt->btn);
		}

		// 3. Procesar el resultado una sola vez
		/* Las tres tablas se indexan con un id que viene del mando, asi que hay
		 * que acotarlo: un indice fuera de rango no daba un fallo visible, daba
		 * un texto basura. Si no hay etiqueta, se cae al numero. */
		if (sdlIdBtn > -1) {
			std::string keyStr = Constant::intToString(sdlIdBtn);
			if (opt->tipoKey == KEY_JOY_BTN && isGamepadXbox) {
				if (sdlIdBtn < SDL_BTN_TO_XBOX_SIZE && SDL_BTN_TO_XBOX[sdlIdBtn][0] != '\0')
					keyStr = std::string(SDL_BTN_TO_XBOX[sdlIdBtn]);
			} else if (isGamepadXbox) {
				if (sdlIdBtn < SDL_HAT_TO_XBOX_SIZE && !SDL_HAT_TO_XBOX[sdlIdBtn].empty())
					keyStr = SDL_HAT_TO_XBOX[sdlIdBtn];
			}
			str = (opt->tipoKey == KEY_JOY_BTN ? opt->description : TipoKeyStr[KEY_JOY_HAT]) + keyStr;
		}

		if (sdlIdAxis > -1) {
			std::string axisStr = Constant::intToString(sdlIdAxis);
			if (isGamepadXbox && sdlIdAxis < SDL_JOY_TO_XBOX_SIZE && !SDL_JOY_TO_XBOX[sdlIdAxis].empty())
				axisStr = SDL_JOY_TO_XBOX[sdlIdAxis];
			str += (str.empty() ? "" : ", ") + TipoKeyStr[KEY_JOY_AXIS] + axisStr;
		}

		// Resetear estado de edicion si estaba activo
		if (opt->changeAsked && (sdlIdBtn > -1 || sdlIdAxis > -1)) {
			opt->changeAsked = false;
			opt->lastTimeAsked = 0;
			status = NORMAL;
		}
		
		/* analogSlot >= 0 ya ha dejado 'str' puesto arriba (incluido su propio "-"),
		 * y para esas opciones sdlIdBtn/sdlIdAxis se quedan a -1 a proposito. */
		if (analogSlot < 0 && sdlIdBtn < 0 && sdlIdAxis < 0){
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

/**
* Coloca el cursor en newPos y recoloca la ventana visible de una vez.
*
* Gemelo de ListMenu::moveTo().  La ventana es [iniPos, endPos) y su tamanyo es
* min(maxLines, listSize), que es lo que fija resetIndexPos(); se recalcula aqui
* en vez de arrastrarlo con endPos - iniPos, asi tambien queda coherente cuando
* los cinco campos se restauran de golpe al volver de un submenu.
*
* El guard de menuActual/status vivia en navegar(); esta aqui para que tambien
* cubra a nextPage/prevPage, que antes lo heredaban por pasar por nextPos().
*
* Antes el desplazamiento se hacia de uno en uno, asi que saltar de pagina
* costaba maxLines-1 pasadas; ahora el ajuste es O(1) en los dos casos.
*/
void GestorMenus::moveTo(int newPos){
	int window;
	int maxIniPos;
	int oldPos;

	if (!menuActual || status == POLLING_INPUTS || status == ASK_SAVESTATES) return;
	if (this->listSize <= 0) return;

	oldPos = this->curPos;

	/* Destino fuera de rango: lo acotamos.  Quien quiera dar la vuelta (ver
	 * navegar) ya llega aqui con el indice envuelto. */
	if (newPos < 0){
		newPos = 0;
	} else if (newPos > this->listSize - 1){
		newPos = this->listSize - 1;
	}

	this->curPos = newPos;
	menuActual->seleccionado = this->curPos;

	window = this->listSize < this->maxLines ? this->listSize : this->maxLines;
	/* maxLines vale 0 hasta la primera resetIndexPos(); con listSize ya
	 * poblado eso dejaria la ventana a 0 y descuadraria iniPos. */
	if (window < 1){
		window = 1;
	}

	/* Solo se mueve la ventana si el cursor se ha salido de ella, que es el
	 * mismo criterio que aplicaba el bucle paso a paso. */
	if (this->curPos < this->iniPos){
		this->iniPos = this->curPos;
	} else if (this->curPos > this->iniPos + window - 1){
		this->iniPos = this->curPos - window + 1;
	}

	/* Y nunca dejamos hueco al final de la lista. */
	maxIniPos = this->listSize - window;
	if (this->iniPos > maxIniPos){
		this->iniPos = maxIniPos;
	}
	if (this->iniPos < 0){
		this->iniPos = 0;
	}

	this->endPos = this->iniPos + window;

	/* Solo si el cursor se ha movido de verdad, igual que antes: estos dos
	 * campos son el estado del texto deslizante del elemento seleccionado, y
	 * tocarlos cuando no se ha movido nada reiniciaria la animacion en cada
	 * pulsacion contra el tope. */
	if (this->curPos != oldPos){
		this->pixelShift = 0;
		this->lastSel = -1;
	}
}

/* Logica de navegacion Arriba/Abajo.  DA LA VUELTA: del ultimo elemento se pasa
 * al primero y del primero al ultimo.  nextPage/prevPage no, se quedan en el
 * extremo, que es lo que hacian antes al toparse con el clamp de navegar(). */
void GestorMenus::navegar(int dir) { // -1 o 1
	if (!menuActual || status == POLLING_INPUTS || status == ASK_SAVESTATES) return;
	if (this->listSize <= 0) return;

	if (dir > 0){
		moveTo(this->curPos >= this->listSize - 1 ? 0 : this->curPos + 1);
	} else if (dir < 0){
		moveTo(this->curPos <= 0 ? this->listSize - 1 : this->curPos - 1);
	}
}

void GestorMenus::nextPos(){
    navegar(1);
}

void GestorMenus::prevPos(){
    navegar(-1);
}

void GestorMenus::nextPage(){
    moveTo(this->curPos + (this->maxLines - 1));
}

void GestorMenus::prevPage(){
    moveTo(this->curPos - (this->maxLines - 1));
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