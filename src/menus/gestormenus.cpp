#include <algorithm> // Imprescindible para std::sort
#include <math.h>

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


SDL_Surface* GestorMenus::imgText;

std::string syncOptionsStrings[TOTAL_VIDEO_SYNC];
std::string aspectRatioStrings[TOTAL_VIDEO_RATIO];
std::string videoScaleStrings[TOTAL_VIDEO_SCALE];
std::string videoShaderStrings[TOTAL_SHADERS];
std::string ACTION_ASK_STR[MAX_ASK];
std::string TipoKeyStr[KEY_JOY_MAX];
std::string FRONTEND_BTN_TXT[MAXJOYBUTTONS];
std::string configurablePortButtonsStr[MAXJOYBUTTONS];
std::string configurablePortHatsStr[MAXJOYBUTTONS];
std::string HOTKEYS_STR[HK_MAX];

extern bool swapToNewDisc(const std::string& newBinPath);
extern bool swapDisc(unsigned new_idx);
extern struct retro_disk_control_callback disk_control;


const char *scrapOrigins[] = {"SCREENSCRAPER", "THEGAMESDB", "EMPTY"};

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
	options_changed_flag = false;

	const int box2dW = screenw / 2 - 2 * marginX;
	const int box2dH = box2dW;
    
	imageMenu.setX(screenw - box2dW - marginX);
    imageMenu.setY(getY() + 3);
    imageMenu.setW(box2dW);
	imageMenu.setH(box2dH);
	askNumOptions = 0;
	scrapGamesSelection = 1;
}

GestorMenus::~GestorMenus() {
	for (std::size_t i = 0; i < cdromListMenu->opciones.size(); ++i) {
		delete ((OpcionTxt *)cdromListMenu->opciones[i])->context;
	}

    for(std::size_t i = 0; i < todosLosMenus.size(); i++) {
		if (todosLosMenus[i])
			delete todosLosMenus[i];
		todosLosMenus[i] = NULL;
	}
}

std::string GestorMenus::guardarJoysticks(Joystick* joy){
	LOG_DEBUG("Guardando valores del joystick");
	return LanguageManager::instance()->get("msg.filesave") + joy->saveButtonsRetroCore();
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
	std::string msg = joy->saveButtonsDefaultsCore();
	return LanguageManager::instance()->get("msg.filesave") + msg;
}

std::string GestorMenus::guardarCoreConfig(CfgLoader *refConfig){
	LOG_DEBUG("Guardando valores del core actual");
	return refConfig->saveCoreParams();
}

std::string GestorMenus::guardarMainConfig(CfgLoader *refConfig){
	LOG_DEBUG("Guardando valores principales de configuracion");
	return refConfig->saveMainParams();
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
			// Obtener el último elemento
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
		// Obtener el último elemento
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
void GestorMenus::inicializar(CfgLoader *refConfig, Joystick *joystick) {
    TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);

	SDL_JOY_TO_XBOX[0] = LanguageManager::instance()->get("menu.controls.left");
	SDL_JOY_TO_XBOX[1] = LanguageManager::instance()->get("menu.controls.right");
	SDL_JOY_TO_XBOX[2] = LanguageManager::instance()->get("menu.controls.up");
	SDL_JOY_TO_XBOX[3] = LanguageManager::instance()->get("menu.controls.down");

	SDL_HAT_TO_XBOX[1] = LanguageManager::instance()->get("menu.controls.up");
	SDL_HAT_TO_XBOX[2] = LanguageManager::instance()->get("menu.controls.right");
	SDL_HAT_TO_XBOX[4] = LanguageManager::instance()->get("menu.controls.down");
	SDL_HAT_TO_XBOX[8] = LanguageManager::instance()->get("menu.controls.left");

	// 1. Crear contenedores de menús
    menuRaiz = new Menu(LanguageManager::instance()->get("menu.main.options"));
    Menu* menuVideo = new Menu(LanguageManager::instance()->get("menu.main.video"), menuRaiz);
	Menu* menuEmulation = new Menu(LanguageManager::instance()->get("menu.main.emulation"), menuRaiz);
	Menu* menuEntrada = new Menu(LanguageManager::instance()->get("menu.main.input"), menuRaiz);
	menuCoreOptions = new Menu(LanguageManager::instance()->get("menu.main.core.options"), menuRaiz);
	menuSavestates = new Menu(LanguageManager::instance()->get("menu.main.saves"), TTF_FontLineSkip(fontMenu), this->getW() / 2 - 2 * marginX, menuRaiz);
	menuScrapper = new Menu(LanguageManager::instance()->get("menu.main.scrapper"), menuRaiz);
	
	Menu* parentAchievements = new Menu(LanguageManager::instance()->get("menu.achievement.title"), menuRaiz);
	//Creamos el submenu que contiene la lista de logros
	const int rowAchHeight = TTF_FontLineSkip(fontMenu) * 2;
	const int menuAchWidth = this->getW() - marginX;
	menuAchievements = new Menu(LanguageManager::instance()->get("menu.achievement.list.title"), rowAchHeight, menuAchWidth, parentAchievements);
	OpcionSubMenu *listaLogros = new OpcionSubMenu(LanguageManager::instance()->get("menu.achievement.list.title"), menuAchievements);
	listaLogros->callback = &GestorMenus::sDescargarLogros;
    listaLogros->context = this;
	parentAchievements->opciones.push_back(listaLogros);
	//Incluimos un indicador para habilitar logros
	parentAchievements->opciones.push_back(new OpcionBool(LanguageManager::instance()->get("menu.achievement.enable"), &refConfig->configMain[cfg::enableAchievements].getBoolRef()));
	//Incluimos un indicador para habilitar el modo hardcore
	OpcionBool *opcionHardcore = new OpcionBool(LanguageManager::instance()->get("menu.achievement.hardcore"), &refConfig->configMain[cfg::hardcoreRA].getBoolRef());
	opcionHardcore->callback = &GestorMenus::changeHardcoreMode;
	parentAchievements->opciones.push_back(opcionHardcore);

	//Este menu no cuelga de ningun lado, pero ponemos partidas guardadas como padre
	menuAskSavestates = new Menu(LanguageManager::instance()->get("menu.savestates.title"), menuSavestates);
        
    todosLosMenus.push_back(menuRaiz);
    todosLosMenus.push_back(menuVideo);
	todosLosMenus.push_back(menuEmulation);
	todosLosMenus.push_back(menuEntrada);
	todosLosMenus.push_back(menuCoreOptions);
	todosLosMenus.push_back(menuSavestates);
	todosLosMenus.push_back(menuAskSavestates);
	todosLosMenus.push_back(menuScrapper);
	todosLosMenus.push_back(parentAchievements);

	//Poblar menu emulacion
	//Escalado de video
    std::vector<std::string> syncvals;
	for (int i=0; i < TOTAL_VIDEO_SYNC; i++){
		syncOptionsStrings[i] = LanguageManager::instance()->get("menu.sync.sync" + Constant::TipoToStr(i));
		syncvals.push_back(syncOptionsStrings[i]);
	}
	menuEmulation->opciones.push_back(new OpcionLista(LanguageManager::instance()->get("menu.options.sync"), syncvals, &refConfig->configMain[cfg::syncMode].getIntRef()));
	menuEmulation->opciones.push_back(new OpcionBool(LanguageManager::instance()->get("menu.options.fps"), &refConfig->configMain[cfg::showFps].getBoolRef()));

	Menu* menuCores = new Menu(LanguageManager::instance()->get("menu.core.assign"), menuEmulation);
	dirutil dir;

	for (unsigned int i=0; i < refConfig->emulators.size(); i++){
		const int nCores = refConfig->emulators[i]->config.cores.size();
		const std::string coreName = refConfig->emulators[i]->config.name;
		const unsigned int cfgIndex = refConfig->findConfigIndex(CfgLoader::coreDefault + refConfig->emulators[i]->config.internalName);

		if (nCores > 1 && cfgIndex > 0){
			std::vector<std::string> coreNames;
			for (int core=0; core < nCores; core++){
				coreNames.push_back(dir.getFileNameNoExt(refConfig->emulators[i]->config.cores[core]));
			}
			OpcionLista *listaCores = new OpcionLista(coreName, coreNames, &refConfig->configMain[cfgIndex].getIntRef());
			listaCores->callback = &GestorMenus::setDefaultEmu;
			listaCores->context = refConfig->emulators[i].get();
			menuCores->opciones.push_back(listaCores);
		}
	}
	menuEmulation->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.core.assign"), menuCores));

	//--------Menu de gestion de discos---------
	menuDisks = new Menu(LanguageManager::instance()->get("menu.disk.control"), menuEmulation);
	cdromListMenu = new Menu(LanguageManager::instance()->get("menu.disk.selectcd"), menuDisks);
	OpcionTxtAndValue* nextCd = new OpcionTxtAndValue(LanguageManager::instance()->get("menu.disk.nextcd"), LanguageManager::instance()->get("menu.disk.nom3u"));
	nextCd->callback = &GestorMenus::cdromNextSelected;

	//Anyadimos la opcion de seleccion de discos
	OpcionSubMenu* diskSubmenu = new OpcionSubMenu(LanguageManager::instance()->get("menu.disk.selectcd"), cdromListMenu);
	diskSubmenu->callback = &GestorMenus::cdromListAction;
	diskSubmenu->context = cdromListMenu;
	menuDisks->opciones.push_back(diskSubmenu);
	menuDisks->opciones.push_back(nextCd);
	menuEmulation->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.disk.control"), menuDisks));
	//--------Menu de gestion de discos---------

    //Poblar Menú Video
	//Relacion de aspecto
	std::vector<std::string> aspectRates;
	for (int i=0; i < TOTAL_VIDEO_RATIO; i++){
		aspectRatioStrings[i] = LanguageManager::instance()->get("menu.aspect.aspect" + Constant::TipoToStr(i));
		aspectRates.push_back(aspectRatioStrings[i]);
	}
	menuVideo->opciones.push_back(new OpcionLista(LanguageManager::instance()->get("menu.options.aspect"), aspectRates, &refConfig->configMain[cfg::aspectRatio].getIntRef()));

	//Escalado de video
    std::vector<std::string> filtros;

#ifdef _XBOX
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
	
	menuVideo->opciones.push_back(new OpcionBool(LanguageManager::instance()->get("menu.options.forcefs"), &refConfig->configMain[cfg::forceFS].getBoolRef()));
	//Animacion del fondo de pantalla del menu
	std::vector<std::string> bgMenu;
	for (int i=0; i < BG_MAX; i++){
		bgMenu.push_back(LanguageManager::instance()->get("menu.background.anim" + Constant::TipoToStr(i)));
	}
	menuVideo->opciones.push_back(new OpcionLista(LanguageManager::instance()->get("menu.background.anim.title"), bgMenu, &refConfig->configMain[cfg::animBG].getIntRef()));
	
	menuAssignRetro = new Menu(LanguageManager::instance()->get("menu.options.paddassign"), menuEntrada);
	Menu* menuAssignFrontend = new Menu(LanguageManager::instance()->get("menu.options.frontassign"), menuEntrada);
	Menu* menuHotkeys = new Menu(LanguageManager::instance()->get("menu.options.hotkeys"), menuEntrada);
	todosLosMenus.push_back(menuAssignRetro);
	todosLosMenus.push_back(menuAssignFrontend);
	todosLosMenus.push_back(menuHotkeys);

	menuEntrada->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.options.paddassign"), menuAssignRetro));
	menuEntrada->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.options.frontassign"), menuAssignFrontend));
	menuEntrada->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.options.hotkeys"), menuHotkeys));
	menuEntrada->opciones.push_back(new OpcionExec<Joystick>(LanguageManager::instance()->get("menu.options.saveassign"), &GestorMenus::guardarJoysticks, joystick, this));
	menuEntrada->opciones.push_back(new OpcionExec<Joystick>(LanguageManager::instance()->get("menu.options.savecoreassign"), &GestorMenus::guardarCoreJoysticks, joystick, this));
	menuEntrada->opciones.push_back(new OpcionExec<Joystick>(LanguageManager::instance()->get("menu.options.savegameassign"), &GestorMenus::guardarGameJoysticks, joystick, this));

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

	//Poblar menu hotkeys
	poblarMenuHotkeys(menuHotkeys, joystick);
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

	// Poblar Menú Principal
    menuRaiz->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.main.video"), menuVideo, ico_video));
	menuRaiz->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.main.emulation"), menuEmulation, ico_settings));
	menuRaiz->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.main.input"), menuEntrada, ico_remap));
	menuRaiz->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.main.core.options"), menuCoreOptions, ico_settings_core));
	menuRaiz->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.main.saves"), menuSavestates, ico_savestates));
	menuRaiz->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.main.scrapper"), menuScrapper, ico_scrapper));
	menuRaiz->opciones.push_back(new OpcionSubMenu(LanguageManager::instance()->get("menu.achievement.title"), parentAchievements, ico_achievements));
	menuRaiz->opciones.push_back(new OpcionExec<CfgLoader>(LanguageManager::instance()->get("menu.main.saveconfig"), &GestorMenus::guardarMainConfig, refConfig, ico_saving, this));
	menuRaiz->opciones.push_back(new OpcionExec<CONFIG_STATUS>(LanguageManager::instance()->get("menu.main.return"), &GestorMenus::volverEmulacion, &status, ico_return, this));
	menuRaiz->opciones.push_back(new OpcionExec<CONFIG_STATUS>(LanguageManager::instance()->get("menu.main.exit"), &GestorMenus::salirEmulacion, &status, ico_shutdown, this));

	// Establecer estado inicial
    menuActual = menuRaiz;
	resetIndexPos();
}

std::string GestorMenus::cdromFileSelected(void* inst, void *value) {
	std::string sendValue = *((std::string *)(value));
	FileProps* pFile = static_cast<FileProps*>(inst);
	LOG_DEBUG("cdromFileSelected: %s", sendValue.c_str());
	swapToNewDisc(pFile->dir + Constant::getFileSep() + pFile->filename);
	delete pFile;
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
	std::unordered_set<std::string> v;
	Constant::splitCharSet(CD_FILTER, ' ', v);
	int nElems = v.size();
	if (v.count(ext) <= 0){
		//El fichero cargado no es un cdrom y por lo tanto salimos
		return;
	}

	vector<unique_ptr<FileProps>> files;
	dir.listarFilesSuperFast(dir.getFolder(ruta).c_str(), files, CD_FILTER, "", true, false);
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
    for (unsigned int i = 0; i < menuAchievements->opciones.size(); i++) {
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
        // Creamos la opción para el menú
        menuAchievements->opciones.push_back(new OpcionAchievement(*currentAch));
    }
    resetIndexPos();
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
	for (unsigned int i=0; i < menuAssignRetro->opciones.size(); i++){
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
    auto& params = refConfig->startupLibretroParams;
    
    // Estructura temporal para ordenar
    struct TempElem {
        std::string key;
        std::string desc;
    };
    std::vector<TempElem> sorter;
    // 1. Llenamos el vector con la clave y la descripcion
    for (auto it = params.begin(); it != params.end(); ++it) {
        TempElem e = { it->first, it->second->description };
        sorter.push_back(e);
    }

    // 2. Ordenamos por descripcion usando una lambda o funcion estatica
    std::sort(sorter.begin(), sorter.end(), [](const TempElem& a, const TempElem& b) {
        return Constant::compareNoCase(a.desc, b.desc);
    });

	menuCoreOptions->opciones.clear();
	menuCoreOptions->opciones.push_back(new OpcionExec<CfgLoader>(LanguageManager::instance()->get("menu.core.options.save"), &GestorMenus::guardarCoreConfig, refConfig, this));
	menuCoreOptions->opciones.push_back(new OpcionTxtAndValue(LanguageManager::instance()->get("menu.core.options.version"), string(refConfig->configMain[cfg::libretro_core].valueStr) + " " + refConfig->configMain[cfg::libretro_core_version].valueStr));
	menuCoreOptions->opciones.push_back(new OpcionTxtAndValue(LanguageManager::instance()->get("menu.core.options.extensions"), refConfig->configMain[cfg::libretro_core_extensions].valueStr));

    // 3. Ahora recorremos el vector ordenado y buscamos en el mapa original por KEY
    for (auto it = sorter.begin(); it != sorter.end(); ++it) {
        auto elem = params.find(it->key);
        if (elem != params.end()) {
            LOG_INFO("Key: %s, Selected: %d", elem->first.c_str(), elem->second->selected);
            menuCoreOptions->opciones.push_back(new OpcionLista(elem->second->description, elem->second->values, &elem->second->selected));
        }
    }
}

/**
*
*/
void GestorMenus::poblarPartidasGuardadas(CfgLoader *refConfig, std::string rompath){
	this->lastImagePath = "";
	imageMenu.closeImage();

	dirutil dir;
	const std::string statesDir = refConfig->configMain[cfg::libretro_state].valueStr + Constant::getFileSep() +
		refConfig->configMain[cfg::libretro_core].valueStr;
	const std::string keyToFind = STATE_EXT;
	std::string filterName = dir.getFileNameNoExt(rompath) + keyToFind;

	#ifdef _XBOX
	//Filtramos nombres largos o caracteres extraños. sumamos un - para contemplar el tamanyo anyadido de los estados numerados
	filterName = dir.getFileNameNoExt(Constant::checkPath(statesDir + Constant::getFileSep() + filterName + "-"));
	#endif

	std::size_t pos = 0;
	std::string posSlot = "0";
	int found = -1;
	vector<unique_ptr<FileProps>> files;

	dir.listarFilesSuperFast(statesDir.c_str(), files, "", filterName, true, true);
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

		// Extraer índice de la ranura
		int iPosSlot = 0;
		if (pos + keyToFind.length() < files[i]->filename.length()){
			posSlot = files[i]->filename.substr(pos + keyToFind.length());
			iPosSlot = Constant::strToTipo<int>(posSlot);
		} else {
			posSlot = "0";
		}

		if (iPosSlot >= 0 && iPosSlot < (int)menuSavestates->opciones.size()) {
			// Usar un puntero temporal para legibilidad y evitar múltiples casteos
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

// Lógica para cambiar valores (Izquierda / Derecha)
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

// Lógica para confirmar (Botón A)
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

    Opcion* opt = menuActual->opciones[menuActual->seleccionado];
    if (opt->tipo == OPC_SUBMENU) {
        menuActual = ((OpcionSubMenu*)opt)->destino;
		resetIndexPos();
		Opcion* e = (Opcion*)opt;
		return e->ejecutar();
    } else if (opt->tipo == OPC_BOOLEANA) {
        cambiarValor(1);
	} else if (opt->tipo == OPC_KEY) {
		OpcionKey* k = (OpcionKey*)opt;
		k->changeAsked = true;
		k->lastTimeAsked = SDL_GetTicks();
		status = POLLING_INPUTS;
	} else if (opt->tipo == OPC_EXEC || opt->tipo == OPC_SAVESTATE || opt->tipo == OPC_SHOW_TXT || opt->tipo == OPC_SHOW_TXT_VAL) {
		Opcion* e = (Opcion*)opt;
		return e->ejecutar();
	} 

	return std::string("");
}

// Lógica para volver (Botón B)
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
		resetIndexPos();
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
void GestorMenus::updateAxis(int sdlAxisValue, int sdlAxis){
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
					//Ponemos a true la opcion "Eje analógico como pad"
					OpcionBool* b = (OpcionBool*)menuActual->opciones[0];
					*(b->valor) = true;
				}
			} else {
				// CENTRO: Opcionalmente manejar el reposo aquí si es necesario
			}
		}
	}
}

/**
*
*/
void GestorMenus::updateButton(int sdlbtn, TipoKey tipoKey){
	if (status != POLLING_INPUTS) return;
	Opcion* opt = menuActual->opciones[menuActual->seleccionado];

	if (opt->tipo == OPC_KEY) {
		OpcionKey* k = static_cast<OpcionKey*>(opt);
		if (k && k->joyInputs) {
			k->description = TipoKeyStr[tipoKey];
			k->tipoKey = tipoKey;

			if (k->tipoKey == KEY_JOY_BTN){
				k->joyMapper->setBtnFromSdl(k->gamepadId, sdlbtn, k->btn);
			} else if (k->tipoKey == KEY_JOY_HAT || k->tipoKey == KEY_JOY_AXIS){
				// Extraemos la dirección activa del Hat (limpiamos otros bits si fuera necesario)
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
	static const int bkg = SDL_MapRGBA(video_page->format, bkgMenu.r, bkgMenu.g, bkgMenu.b, 0xFF);
	static const int iwhite = SDL_MapRGBA(video_page->format, white.r, white.g, white.b, 0xFF);
	static const int iblack = SDL_MapRGBA(video_page->format, black.r, black.g, black.b, 0xFF);
	Icons icons;

	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	int face_h = menuActual->rowHeight;

	Constant::drawTextCentTransparent(video_page, fontMenu, this->menuActual->titulo.c_str(), 0, face_h < marginY ? (marginY - face_h) / 2 : 0 , 
				true, false, textColor, 0);
	fastline(video_page, marginX, marginY - 1, video_page->w - marginX, marginY - 1, textColor);

    //To scroll one letter in one second. We use the face_h because the width of 
    //a letter is not fixed.
    const float pixelsScrollFps = std::max(ceil(face_h / (float)textFps), 1.0f);

    for (int i=this->iniPos; i < this->endPos; i++){
        auto option = this->menuActual->opciones.at(i);
       
		std::string line;
		std::string value;

		if (option->tipo == OPC_SAVESTATE){
			drawSavestateWithImage(i, (OpcionSavestate *) option, video_page);
			continue;
		} else if (option->tipo == OPC_ACHIEVEMENT){
			drawAchievement(i, (OpcionAchievement *) option, video_page);
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
		} else if (option->tipo == OPC_SHOW_TXT_VAL){
			line = option->titulo;
			value = ((OpcionTxtAndValue *)option)->valor;
		} else {
			line = option->titulo;
		}

		const int screenPos = i - this->iniPos;
        const int fontHeightRect = screenPos * face_h;
        const int lineBackground = -1;
        SDL_Color lineTextColor = i == this->curPos ? black : white;

        //Drawing a faded background selection rectangle
        if (i == this->curPos){
            int y = this->getY() + fontHeightRect;
            //Gaining some extra fps when the screen resolution is low
			SDL_Rect rectElem = {this->getX(), y, this->getW() - marginX, face_h};
            if (video_page->h >= 480){
				DrawRectAlpha(video_page, rectElem, bkgMenu, 190);
            } else {
                lineTextColor = white;
            }
			rect(video_page, rectElem.x - 1, rectElem.y - 1, rectElem.x + rectElem.w, rectElem.y + rectElem.h, bkgMenu);
        }
        
		int marginIco = 0;

		if (option->icon > -1 && option->icon < max_icons){
			marginIco = icons.icon_w_add * 2 + 5;
			SDL_Rect dstRect = {this->getX() - icons.icon_w_add + 2, this->getY() + fontHeightRect - icons.icon_w_add / 2, 0, 0};
			SDL_BlitSurface(icons.icons[option->icon], NULL, video_page, &dstRect);
		}

        Constant::drawTextTransparent(video_page, fontMenu, line.c_str(), this->getX() + marginIco, 
                    this->getY() + fontHeightRect, lineTextColor, lineBackground);

		if (option->tipo == OPC_KEY && !((OpcionKey *)option)->description.empty()){
			drawKeys(i, (OpcionKey *)option, video_page);
		} else if (option->tipo == OPC_BOOLEANA){
			drawBooleanSwitch(i, (OpcionBool *)option, video_page);			
		} else if (!value.empty()){
			int pixelDato;
			TTF_SizeUTF8(fontMenu, value.c_str(), &pixelDato, NULL);
			Constant::drawTextTransparent(video_page, fontMenu, value.c_str(), this->getX() + this->getW() - marginX - pixelDato - 1, 
                    this->getY() + fontHeightRect, lineTextColor, lineBackground);
		}
    }

	drawAskMenu(video_page);
}

void GestorMenus::drawKeys(int i, OpcionKey *opt, SDL_Surface *video_page){
	std::string str = "";
	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	const int face_h = TTF_FontLineSkip(fontMenu);
	const int screenPos = i - this->iniPos;
    const int fontHeightRect = screenPos * face_h;
	SDL_Color lineTextColor = i == this->curPos ? black : white;

	std::string titulo = this->menuActual->titulo;
	Constant::lowerCase(&titulo);
	//bool isGamepadXbox = titulo.find("xbox") != std::string::npos;
	bool isGamepadXbox = true;

	// 1. Manejo del temporizador (Early exit)
	Uint32 elapsed = SDL_GetTicks() - opt->lastTimeAsked;
	if (opt->changeAsked && elapsed < 4000) {
		str = LanguageManager::instance()->get("menu.inputs.waitkeypress") + Constant::intToString((5000 - elapsed) / 1000) + " s";
	} else {
		// 2. Obtener el ID de SDL según el tipo de entrada
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

		// Resetear estado de edición si estaba activo
		if (opt->changeAsked && (sdlIdBtn > -1 || sdlIdAxis > -1)) {
			opt->changeAsked = false;
			opt->lastTimeAsked = 0;
			status = NORMAL;
		}
		
		if (sdlIdBtn < 0 && sdlIdAxis < 0){
			str = "-";
		}
	}

	int pixelDato;
	TTF_SizeText(fontMenu, str.c_str(), &pixelDato, NULL);
	Constant::drawTextTransparent(video_page, fontMenu, str.c_str(), this->getX() + this->getW() - marginX - pixelDato - 1, 
            this->getY() + fontHeightRect, lineTextColor, 0);
}

void GestorMenus::drawBooleanSwitch(int i, OpcionBool *opcion, SDL_Surface *video_page){
	// 1. Extraer el valor y definir dimensiones base
	bool enabled = *opcion->valor;
	const int iblack = SDL_MapRGB(video_page->format, black.r, black.g, black.b);
	const int iswitchenabled = SDL_MapRGB(video_page->format, 200, 200, 200);
	const int iswitchdisabled = SDL_MapRGB(video_page->format, 77, 77, 77);
	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	const int face_h = TTF_FontLineSkip(fontMenu);
	const int screenPos = i - this->iniPos;
    const int fontHeightRect = screenPos * face_h;
	const int sw_h = face_h - 5;
	const int sw_w = 50;
	const int sw_x = getX() + getW() - marginX - sw_w;
	const int sw_y = getY() + fontHeightRect + 2;

	// 2. Dibujar el fondo del switch
	SDL_Rect baseRect = { sw_x, sw_y, sw_w, sw_h };
	SDL_FillRect(video_page, &baseRect, enabled ? iswitchenabled : iswitchdisabled);

	// 3. Calcular el thumb (botón interno) de forma relativa
	const int spacing = 4;
	const int size = sw_h - (spacing * 2);
	int thumbX = sw_x + (enabled ? (sw_w - size - spacing) : spacing);

	SDL_Rect thumbRect = { thumbX, sw_y + spacing, size, size };

	// 4. Dibujar el thumb según el estado
	if (enabled) {
		SDL_FillRect(video_page, &thumbRect, iblack);
	} else {
		// Usando los campos de thumbRect directamente para evitar sumas manuales
		rect(video_page, thumbRect.x, thumbRect.y, thumbRect.x + size, thumbRect.y + size, black);
		rect(video_page, thumbRect.x + 1, thumbRect.y + 1, thumbRect.x + size - 1, thumbRect.y + size - 1, black);
	}
}

void GestorMenus::drawAskMenu(SDL_Surface *video_page) {
    // 1. Colores estáticos (usamos SDL_Color y el int mapeado según necesidad)
    static const int iaskClBg = SDL_MapRGB(video_page->format, askClBg.r, askClBg.g, askClBg.b);
    static const int iaskClLine = SDL_MapRGB(video_page->format, askClLine.r, askClLine.g, askClLine.b);
    static const int iaskClTitle = SDL_MapRGB(video_page->format, askClTitle.r, askClTitle.g, askClTitle.b);
    static const int iaskClText = SDL_MapRGB(video_page->format, askClText.r, askClText.g, askClText.b);
    
    if (status != ASK_SAVESTATES) return;

    TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
    const int ask_w = 520, ask_h = 200, btn_h = 30, btn_w = 150, marginTitle = 10;
    int face_h = TTF_FontLineSkip(fontMenu);

    SDL_Rect thumbRect = { (this->w - ask_w) / 2, (this->h - ask_h) / 2, ask_w, ask_h };
    SDL_Rect titleRect = { thumbRect.x, thumbRect.y, thumbRect.w, 40 };

    // Dibujado de fondo y bordes (agrupado)
    SDL_FillRect(video_page, &thumbRect, iaskClBg);
    SDL_FillRect(video_page, &titleRect, iaskClTitle);
    rect(video_page, thumbRect.x, thumbRect.y, thumbRect.x + thumbRect.w, thumbRect.y + thumbRect.h, askClLine);
    rect(video_page, thumbRect.x - 1, thumbRect.y - 1, thumbRect.x + thumbRect.w + 1, thumbRect.y + thumbRect.h + 1, askClLine);

    Opcion* opt = menuAskSavestates->opciones[0];
    Constant::drawTextTransparent(video_page, fontMenu, opt->titulo.c_str(), 
                                 titleRect.x + marginTitle, titleRect.y + (titleRect.h - face_h) / 2, askClText, 0);

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
            // Simplificación lógica: Si es onlySave, saltar índices que no sean ASK_GUARDAR
            if (onlySave && i != ASK_GUARDAR) continue;

            bool isSelected = (i == *(l->indice));
            
            // Colores según selección
            SDL_Color clText = isSelected ? askClTitle : askClText;
            int clBg = isSelected ? iaskClText : iaskClLine;

            SDL_Rect btnRect = { titleRect.x + 10 + ((btn_w + freeSpace) * i), btnY, btn_w, btn_h };

            // Dibujar botón
            SDL_FillRect(video_page, &btnRect, clBg);
            rect(video_page, btnRect.x, btnRect.y, btnRect.x + btnRect.w, btnRect.y + btnRect.h, clText);

            // Centrar texto en el botón
            int textW;
            TTF_SizeText(fontMenu, l->items[i].c_str(), &textW, NULL);
            Constant::drawTextTransparent(video_page, fontMenu, l->items[i].c_str(), 
                                         btnRect.x + (btn_w - textW) / 2, btnRect.y + (btn_h - face_h) / 2, clText, 0);
        }
    }
}

/**
*
*/
void GestorMenus::drawSelectionBox(int i, SDL_Surface *video_page, SDL_Color& lineTextColor){
	int face_h = menuActual->rowHeight;
	const int screenPos = i - this->iniPos;
    const int fontHeightRect = screenPos * face_h;
	lineTextColor = i == this->curPos ? black : white;

	std::string rutaSelected;
	if (i == this->curPos){
        int y = this->getY() + fontHeightRect;
        //Gaining some extra fps when the screen resolution is low
		SDL_Rect rectElem = {this->getX(), y, menuActual->menuWidth, face_h};
        if (video_page->h >= 480){
			DrawRectAlpha(video_page, rectElem, bkgMenu, 190);
        } else {
            lineTextColor = white;
        }
		//Drawing the selection menu
		rect(video_page, rectElem.x - 1, rectElem.y - 1, rectElem.x + rectElem.w, rectElem.y + rectElem.h, bkgMenu);
    } 
}

/**
*
*/
void GestorMenus::drawAchievement(int i, OpcionAchievement *opcion, SDL_Surface *video_page){
	SDL_Color lineTextColor = i == this->curPos ? black : white;
	drawSelectionBox(i, video_page, lineTextColor);

	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	TTF_Font *fontSmall = Fonts::getFont(Fonts::FONTSMALL);
	const int face_h = TTF_FontLineSkip(fontMenu);
	const int screenPos = i - this->iniPos;
	const int fontHeightRect = screenPos * menuActual->rowHeight;
	const int marginImg = 2;
	const int imgH = menuActual->rowHeight - 2*marginImg;

	const int position = this->getY() + fontHeightRect;
	if (opcion->achievement.isSection){
		const std::string s = "----- " + opcion->achievement.title + " -----";
		Constant::drawTextTransparent(video_page, fontMenu, s.c_str(), 
			this->getX() + imgH + marginImg * 3, 
			position + menuActual->rowHeight / 2 - face_h / 2, 
			i == this->curPos ? black : blue);
	} else {
		std::string firstLine = opcion->achievement.title;
		if (opcion->achievement.points > 0){
			firstLine += " (" + Constant::TipoToStr(opcion->achievement.points) + " point" + (opcion->achievement.points > 1 ? "s" : "") + ")";
		}
		//Drawing the first line of text on big font
		Constant::drawTextTransparent(video_page, fontMenu, firstLine.c_str(), this->getX() + imgH + marginImg * 3, position, lineTextColor);
		//Drawing the second line of text on a smaller font
		Constant::drawTextTransparent(video_page, fontSmall, opcion->achievement.description.c_str(), this->getX() + imgH + marginImg * 3, position + face_h, lineTextColor);
	}

    // Solo intentamos añadir a la cola si NO tiene imagen Y NO se está descargando ya
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

	// Dibujar el badge si ya está descargado
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
	const int face_h = TTF_FontLineSkip(fontMenu);
	const int face_h_small = TTF_FontLineSkip(fontSmall);
    const int screenPos = i - this->iniPos;
    const int fontHeightRect = screenPos * face_h;
    const int lineBackground = -1;
    SDL_Color lineTextColor = i == this->curPos ? black : white;
	const std::string line = opcion->titulo;
	std::string rutaSelected;

	drawSelectionBox(i, video_page, lineTextColor);

	if (i == this->curPos){
		rutaSelected = opcion->file.filename;
		//Drawing the modification date
		if (!opcion->file.modificationTime.empty()){
			//Drawing below the image
			Constant::drawTextTransparent(video_page, fontSmall, std::string(LanguageManager::instance()->get("menu.savestate.latestsave") 
				+ opcion->file.modificationTime).c_str(), imageMenu.getX(), 
                imageMenu.getY() + imageMenu.getH() + 2, white, 0);
		}
    } else {
		if (opcion->file.modificationTime.empty()){
			lineTextColor = menuBars;
		}
	}

	//Drawing the text
    Constant::drawTextTransparent(video_page, fontMenu, line.c_str(), this->getX(), 
                this->getY() + fontHeightRect, lineTextColor, lineBackground);
	
	//Drawing the image
	if (!rutaSelected.empty() && lastImagePath != rutaSelected){

		std::string rutaImg = opcion->file.dir + Constant::getFileSep() + opcion->file.filename + STATE_IMG_EXT;
		#ifdef _XBOX
		//Filtramos nombres largos o caracteres extraños
		rutaImg = Constant::checkPath(rutaImg);
		#endif
		imageMenu.loadImage(rutaImg);
		lastImagePath = opcion->file.filename;
	}

	//Drawing the date besides the text
	if (!opcion->file.modificationTime.empty()){
		Constant::drawTextTransparent(video_page, fontSmall, opcion->file.modificationTime.c_str(), this->getX() + 120, 
                this->getY() + fontHeightRect + face_h_small / 3, lineTextColor, lineBackground);
	}
	
	if (!rutaSelected.empty() && !opcion->file.modificationTime.empty()){
		imageMenu.printImage(video_page);
	}
	//rect(video_page, imageMenu.getX(), imageMenu.getY(), imageMenu.getX() + imageMenu.getW(), imageMenu.getY() + imageMenu.getH(), white);
}

/**
*
*/
int GestorMenus::getScreenNumLines(){
	if (this->menuActual != NULL){
		const int face_h = this->menuActual->rowHeight;
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
void GestorMenus::nextPos(){
	this->navegar(1);
	this->curPos = menuActual->seleccionado;
}

void GestorMenus::prevPos(){
	this->navegar(-1);
	this->curPos = menuActual->seleccionado;
}
*/

// Lógica de navegación Arriba/Abajo
void GestorMenus::navegar(int dir) { // -1 o 1
    if (!menuActual || status == POLLING_INPUTS || status == ASK_SAVESTATES) return;

    /*int num = (int)menuActual->opciones.size();
	if (num > 0){
		menuActual->seleccionado = (menuActual->seleccionado + dir + num) % num;
	}*/

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