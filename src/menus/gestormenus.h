#define NOMINMAX
#include <uiobjects/object.h>
#include <io/cfgloader.h>
#include <io/joystick.h>
#include <io/dirutil.h>
#include <uiobjects/image.h>
#include <beans/structures.h>
#include <font/fonts.h>
#include <http/badgedownloader.h>
#include <const/cfgconst.h>
#include <http/gamefaqs.h>

#include <iostream>
#include <vector>
#include <string>

// --- Definici�n de tipos de opciones ---
enum TipoOpcion { OPC_BOOLEANA, OPC_LISTA, OPC_LISTA_REF, OPC_SUBMENU, OPC_INT, OPC_KEY, OPC_EXEC, OPC_SHOW_TXT, OPC_SHOW_TXT_VAL, OPC_SHOW_DYNTXT_VAL, OPC_SAVESTATE, OPC_ACHIEVEMENT, 
	OPC_FAQ_SEARCH, OPC_FAQ_SELECT, OPC_FAQ_TXT, OPC_SHOW_IMG, OPC_UNDEFINED};

/* KEY_JOY_ANALOG: la opcion apunta a una DIRECCION ANALOGICA del core, no a un
 * boton digital. Se lee de mapperCore.analogDst, no de las tres tablas normales. */
enum TipoKey{KEY_JOY_BTN,KEY_JOY_HAT,KEY_JOY_AXIS, KEY_JOY_ANALOG, KEY_JOY_MAX};
enum ACTION_ASK{ASK_CARGAR, ASK_GUARDAR, ASK_ELIMINAR, MAX_ASK};
enum CONFIG_STATUS{NORMAL,POLLING_INPUTS,ASK_SAVESTATES, EXIT_CONFIG, EXIT_EMULATION, START_SCRAPPING, MAX_CONFIG_STATUS};

const uint8_t BOOT_NO_DISK = 0x01;
const uint8_t BOOT_WITH_DISK = 0x02;


struct t_option_action{
	TipoOpcion option;
	int action;
	void *elem;
	int indexSelected;
	std::string message;
	
	t_option_action(){
		option = OPC_BOOLEANA;
		action = 0;
		elem = NULL;
		indexSelected = 0;
	}
};

struct t_scrap{
	bool selected;
	int index;
	string name;

	t_scrap(){
		selected = false;
		index = 0;
	}	
};

struct t_save_override{
	int emuIdx;
	CfgLoader *refConfig;
	t_save_override(int idx, CfgLoader * cfg){
		emuIdx = idx;
		refConfig = cfg;
	}
};


struct Menu; // Declaracion anticipada
class OpcionSavestate;
class OpcionBool;

// Definimos un tipo de funcion que devuelve string y no recibe nada
typedef std::string (*GestorCallback)(void* instance);
typedef std::string (*CallbackValue) (void* instance, void* value);
typedef std::string (*CallbackValues)(void* instance, void* index, void* values);

// Clase Base para las opciones del menu
class Opcion {
public:
    std::string titulo;
    TipoOpcion tipo;
	int icon;
	bool editable;

    Opcion(std::string t, TipoOpcion tp) : titulo(t), tipo(tp), icon(-1), editable(false) {}
	Opcion(std::string t, TipoOpcion tp, int ico) : titulo(t), tipo(tp), icon(ico), editable(false) {}
	virtual std::string ejecutar() = 0; // Metodo virtual puro
    virtual ~Opcion() {}
};

//Esta clase no permite modificar ningun valor, solo muestra texto
class OpcionTxt : public Opcion {
public:
	std::string valor;
	CallbackValue callback; // Funcion estatica
    void* context;
    OpcionTxt(std::string t) : Opcion(t, OPC_SHOW_TXT), valor(t), callback(NULL), context(NULL) {}

	std::string ejecutar() override {
		if (callback != NULL) {
            return callback(context, (void *)&valor); 
        }
        return "";
    }
};

class OpcionGameFaq : public Opcion {
public:
	GameResult valor;
	CallbackValue callback; // Funcion estatica
    void* context;
    OpcionGameFaq(GameResult t) : Opcion(t.name, OPC_FAQ_SEARCH), valor(t), callback(NULL), context(NULL) {}

	std::string ejecutar() override {
		if (callback != NULL) {
            return callback(context, (void *)&valor); 
        }
        return "";
    }
};

class OpcionFaq : public Opcion {
public:
	GuidesResult valor;
	CallbackValue callback;
    void* context;

    OpcionFaq(GuidesResult t) : Opcion(t.name, OPC_FAQ_SELECT), valor(t), callback(NULL), context(NULL) {}
	
	OpcionFaq(std::string categ) : Opcion(categ, OPC_FAQ_SELECT), callback(NULL), context(NULL) {
		valor.name = categ;
	}

	std::string ejecutar() override {
		if (callback != NULL) {
            return callback(context, (void *)&valor); 
        }
        return "";
    }
};

class OpcionImage : public Opcion {
public:
	std::string url;
	CallbackValue callback; // Funcion estatica
	

    void* context;
    OpcionImage(std::string t) : Opcion(t, OPC_SHOW_IMG), url(t), callback(NULL), context(NULL) {}

	std::string ejecutar() override {
		if (callback != NULL) {
            return callback(context, (void *)&url); 
        }
        return "";
    }
};

//Esta clase no permite modificar ningun valor, solo muestra texto y un valor
class OpcionTxtAndValue : public Opcion {
public:
	std::string valor;
	CallbackValue callback; // Funcion estatica
    void* context;
	bool isPassword;
	std::string tmpValue;
	cfg::MAIN_CFG_PROPS_KEYS cfgKey;

    OpcionTxtAndValue(std::string t, std::string v) : Opcion(t, OPC_SHOW_TXT_VAL), valor(v), tmpValue(v), cfgKey(cfg::MAIN_CFG_MAX), callback(NULL), context(NULL), isPassword(false) {}
	OpcionTxtAndValue(std::string t, cfg::MAIN_CFG_PROPS_KEYS k) : Opcion(t, OPC_SHOW_TXT_VAL), cfgKey(k), callback(NULL), context(NULL), isPassword(false) {
		CfgLoader::configMain[k].getPropValue(valor);
		tmpValue = valor;
	}

	void setPassword(bool b){
		isPassword = b;
		if (b) valor = PASS_MASK;
	}

	std::string ejecutar() override {
		if (callback != NULL) {
            return callback(context, (void *)&valor); 
        }
        return "";
    }
};

class OpcionTxtAndDynValue : public Opcion {
public:
    const std::string* valor; // Puntero al string estático/dinámico externo
    CallbackValue callback;
    void* context;

    // El constructor recibe el string externo por referencia y guarda su dirección (&v)
    OpcionTxtAndDynValue(std::string t, const std::string& v) 
        : Opcion(t, OPC_SHOW_DYNTXT_VAL), valor(&v), callback(NULL), context(NULL) {}

    std::string ejecutar() override {
        if (callback != NULL) {
            // Pasamos la dirección del string original (que es lo que almacena el puntero)
            return callback(context, (void *)valor); 
        }
        return "";
    }
};

class OpcionAchievement : public Opcion {
public:
	AchievementState achievement;

	OpcionAchievement(AchievementState ach) : Opcion(ach.title, OPC_ACHIEVEMENT) {
		achievement = ach;
	}

	~OpcionAchievement(){
		//Liberamos las dos memorias
		if (achievement.badgeLocked != NULL){
			SDL_FreeSurface(achievement.badgeLocked);
			achievement.badgeLocked = NULL;
		}
		if (achievement.badge != NULL){
			SDL_FreeSurface(achievement.badge);
			achievement.badge = NULL;
		}
	}

	std::string ejecutar() override {
        return "";
    }
};

class OpcionSavestate : public Opcion {
public:
	FileProps file;
	CONFIG_STATUS *status;
	OpcionSavestate(std::string t) : Opcion(t, OPC_SAVESTATE), status(NULL) {}

	std::string ejecutar() override {
		LOG_DEBUG("Selecting %s", file.filename.c_str());
		if (status){
			*status = ASK_SAVESTATES;
		}
	    return "";
    }
};


class OpcionBool : public Opcion {
public:
    bool* valor;
	CallbackValue callback; // Funcion estatica
    void* context;          // El "this" de GestorMenus

    OpcionBool(std::string t, bool* v, int ico = -1) : Opcion(t, OPC_BOOLEANA, ico), valor(v), callback(NULL), context(NULL) {};

	std::string ejecutar() override {
		if (callback != NULL && valor != NULL) {
            return callback(context, (void *)valor); 
        }
        return "";
    }
};

class OpcionInt : public Opcion {
public:
    int* valor;
	std::string description;
	std::string format;
	int divisor;
	bool allowNegative;

    OpcionInt(std::string t, int* v, std::string f, int div) : Opcion(t, OPC_INT), valor(v), format(f) {
		divisor = div == 0 ? 1 : div;
		description = Constant::string_format(f, *v / (float)divisor);
		allowNegative = false;
	}
	std::string ejecutar() override {
        return "";
    }
};

class OpcionListaCommon : public Opcion {
public:
	int* indice;
	CallbackValues callback;
	void* context;

protected:
    OpcionListaCommon(std::string t, TipoOpcion tipo, int* idx)
        : Opcion(t, tipo), indice(idx), callback(NULL), context(NULL) {}

    std::string ejecutarConItems(void* itemsPtr) {
        if (callback != NULL && indice != NULL)
            return callback(context, (void*)indice, itemsPtr);
        return "";
    }
};

class OpcionLista : public OpcionListaCommon {
public:
    std::vector<std::string> items;

    OpcionLista(std::string t, std::vector<std::string> it, int* idx)
        : OpcionListaCommon(t, OPC_LISTA, idx), items(it) {}

    std::string ejecutar() override {
        return ejecutarConItems((void*)&items);
    }
};

class OpcionListaRef : public OpcionListaCommon {
public:
    std::vector<std::string>* items;

    OpcionListaRef(std::string t, std::vector<std::string>* it, int* idx)
        : OpcionListaCommon(t, OPC_LISTA_REF, idx), items(it) {}

    std::string ejecutar() override {
        return ejecutarConItems((void*)items);
    }
};

class OpcionSubMenu : public Opcion {
public:
	//Menu que contiene las opciones
    Menu* destino;
	//Funcion estatica
	GestorCallback callback;
	//Parametros que se le pasan a la funcion estatica
    void* context;

    OpcionSubMenu(std::string t, Menu* d) : Opcion(t, OPC_SUBMENU), destino(d), callback(NULL), context(NULL){}
	OpcionSubMenu(std::string t, Menu* d, int ico) : Opcion(t, OPC_SUBMENU, ico), destino(d), callback(NULL), context(NULL) {}

	std::string ejecutar() override {
        if (callback != NULL && context != NULL) {
            return callback(context); 
        }
        return "";
    }
};

class OpcionKey : public Opcion {
public:
    t_joy_state* joyInputs;
	t_joy_mapper * joyMapper;
	int *intRef; 
	int btn;
	int gamepadId;
	std::string description;
	bool changeAsked; 
	Uint32 lastTimeAsked;
	TipoKey tipoKey;

	OpcionKey(std::string t, t_joy_state *pjoyInputs, t_joy_mapper * pjoyMapper, int pgamepadId, int pBtn, TipoKey ptipoKey, std::string desc, int ico = -1): Opcion(t, OPC_KEY){
		btn = pBtn;
		gamepadId = pgamepadId;
		tipoKey = ptipoKey;
		description = desc;
		lastTimeAsked = 0;
		changeAsked = false;
		joyInputs = pjoyInputs;
		joyMapper = pjoyMapper;
		intRef = NULL;
		this->icon = ico;
	}
	
	std::string ejecutar() override {
        return "";
    }
};

// Estructura del Menu
struct Menu{
    std::string titulo;
    std::vector<Opcion*> opciones;
    int seleccionado;
    Menu* padre;
	int rowHeight;
	int menuWidth;

    Menu(std::string t, Menu* p = NULL) : titulo(t), seleccionado(0), padre(p) {
		rowHeight = Fonts::getLineSkip(Fonts::FONTBIG);
	}
	
	Menu(std::string t, int rh, int mw, Menu* p = NULL) : titulo(t), seleccionado(0), padre(p) {
		rowHeight = rh;
		menuWidth = mw;
	}

    ~Menu() {
        for(std::size_t i = 0; i < opciones.size(); i++) delete opciones[i];
    }
};

struct GameFaqsMenu{
	std::string gameName;
	GameFaqs gameFaqs;
};

struct MenuStatus{
	int iniPos;
    int endPos;
    int curPos;
    int maxLines;
	int listSize;
	int selectedMenuPos;
	Menu *menu;
};

// --- Clase Principal de Gestion de Menus ---
class GestorMenus : public Object{
private:
    Menu* menuRaiz;     // Menu principal (almacenado permanentemente)
    Menu* menuActual;   // Puntero al menu que se esta mostrando ahora
    static const int waitTitleMove = 2000;
	static const int textFps = 20;
	static const int frameTimeText = (int)(1000 / textFps);

	struct AskUserData {
		std::string* valorPtr;
		CfgLoader* config;
		bool isPassword;
		cfg::MAIN_CFG_PROPS_KEYS cfgKey;

		AskUserData(){
			isPassword = false;
			cfgKey = cfg::MAIN_CFG_MAX;
			valorPtr = NULL;
			config = NULL;
		}
	};

    // Lista de todos los menus para liberar memoria al final
    std::vector<Menu*> todosLosMenus;
	Menu* menuCoreOptions;
	// Submenus transitorios por categoria dentro de menuCoreOptions (V2). Se
	// recrean en cada poblarCoreOptions; hay que liberarlos a mano antes de repoblar
	// (OpcionSubMenu no borra su destino). Runtime only.
	std::vector<Menu*> coreOptionSubmenus;
	Menu* menuCheats;
	Menu* menuSavestates;
	Menu* menuAskSavestates;
	Menu* menuScrapper;
	Menu* menuAchievements;
	Menu* menuAssignRetro;
	Menu* menuAssignFrontend;
	int askNumOptions;
	//Menu que rellena el frontend
	Menu* cdromListMenu;
	Menu* menuDisks;
	Menu* menuGuides;
	Menu* menuGuideText;
	Menu *menuOverscan;

	CONFIG_STATUS status;
	int marginX;
    int marginY;
    int iniPos;
    int endPos;
    int curPos;
    int listSize;
    int maxLines;
    int layout;
    bool animateBkg;
    bool centerText;
    int lastSel;
    float pixelShift;
    static SDL_Surface* imgText;
	SDL_Surface* tmpTextOption;
	std::string lastImagePath;
	Image imageSavestate;
	int scrapGamesSelection;
	int face_h_big;
	int face_h_small;
	GameFaqsMenu gameFaqsMenu;
	std::vector<struct MenuStatus> historyMenu;

	int getScreenNumLines();
	void clearSelectedText();
	void setLayout(int layout, int screenw, int screenh);
	void addControlerOptions(Menu*&, int, Joystick *, CfgLoader *);
	void addControlerButtons(Menu*&, int, Joystick *);
	int findAxisPos(int retroDirection);
	void resetKeyElement(int, TipoKey);
	void drawSelectionBox(int i, SDL_Surface *video_page, SDL_Color& lineTextColor, int face_h = 0);
	void drawSavestateWithImage(int, OpcionSavestate *, SDL_Surface *);
	void drawBooleanSwitch(int, OpcionBool *, SDL_Surface *);
	void drawAskMenu(SDL_Surface *video_page);
	void drawKeys(int i, OpcionKey *opt, SDL_Surface *video_page);
	void drawAchievement(int, OpcionAchievement *, SDL_Surface *);
	void drawFaqSearch(int i, OpcionGameFaq *opcion, SDL_Surface *video_page);
	void drawFaqSelect(int i, OpcionFaq *opcion, SDL_Surface *video_page);
	void drawImage(int i, OpcionImage *opcion, SDL_Surface *video_page);
	void drawBordersMenuOverlay(SDL_Surface *video_page);

	void resetAskPosition();
	void poblarMenuScrapper(CfgLoader *refConfig, Menu* menuScrapper);
	void poblarMenuHotkeys(Menu* menuHotkeys, Joystick *joystick);
	void poblarMenuAssignFrontend(Menu* menuHotkeys, Joystick *joystick);
	void poblarMenuRapidFire(Menu* menuRapidFire, Joystick *joystick);
	void poblarMenuCoreOverrides(Menu *menu, CfgLoader *refConfig);
	void poblarMenuVideo(Menu* menuVideo, CfgLoader *refConfig);
	void poblarMenuPad(Menu* menuEntrada, CfgLoader *refConfig, Joystick *joystick);
	void poblarMenuEmulacion(Menu* menuEmulation, CfgLoader *refConfig);
	void poblarMenuAudio(Menu* menuAudio, CfgLoader *refConfig);
	void poblarMenuLogros(Menu* parentAchievements, CfgLoader *refConfig);
	void checkMultipleSystemCore(CfgLoader *refConfig, Menu *menu, int coreIdx);
	void addMusicOptionList(CfgLoader *refConfig, int posCore, std::vector<Opcion*> &opciones);

	std::string guardarJoysticks(Joystick* joy);
	std::string guardarGameJoysticks(Joystick* joy);
	std::string guardarCoreJoysticks(Joystick* joy);
	std::string guardarCoreConfig(CfgLoader *refConfig);
	std::string guardarCoreConfigGame(CfgLoader *refConfig);
	std::string restaurarCoreConfig(CfgLoader *refConfig);
	std::string guardarMainConfig(CfgLoader *refConfig);
	std::string guardarCoreOverridesConfig(t_save_override *overrides);
	std::string reloadCheats(CfgLoader *refConfig);
	std::string descargarCheats(CfgLoader *refConfig);
	std::string volverEmulacion(CONFIG_STATUS *st);
	std::string salirEmulacion(CONFIG_STATUS *st);
	std::string startScrapping(CONFIG_STATUS *st);	
	void addCoreOptionsByCategory(Menu *parent, const std::vector<std::pair<std::string, std::string> > &categories, const std::map<std::string, std::unique_ptr<cfg::t_emu_props> > &params);

public:
    GestorMenus(int screenw, int screenh);
	~GestorMenus();
    // Inicializa la estructura de menus
    void inicializar(CfgLoader *, Joystick *);
    // Logica de navegacion Arriba/Abajo
    void navegar(int dir);
    // Logica para cambiar valores (Izquierda / Derecha)
    void cambiarValor(int dir);
    // Logica para confirmar (Boton A)
    std::string confirmar(t_option_action *);
    // Logica para volver (Boton B)
    void volver();
	void resetIndexPos();
    // Metodo simple para obtener que dibujar
    Menu* obtenerMenuActual();
	void setAchievementsAsSelected(){menuActual = menuAchievements;}
	void draw(SDL_Surface *video_page);
	void updateButton(const SDL_Event &event, TipoKey);
	void updateAxis(const SDL_Event &event);
	bool options_changed_flag;
	std::vector<t_scrap> scrapSelection;
	void poblarCoreOptions(CfgLoader *);
	void poblarCheats(CfgLoader *);
	void poblarPartidasGuardadas(CfgLoader *, std::string);
	void poblarJoystickTypes(Joystick *joystick);
	void poblarMenuOverscan(Menu *menu);
	void poblarMenuDiscos(int options);
	void setGameLoaded(std::string gn){
		this->gameFaqsMenu.gameName = gn;
	}

	Menu* menuGameFilter;
	void iniciarFiltros(GameDataFields& gameDataFieldsFilter);

	std::string stopScrapping(CONFIG_STATUS *st);
	void loadAchievements();

	 // True si estamos en el menu de opciones de core O en cualquiera de sus submenus
	 // de categoria (todos cuelgan de menuCoreOptions via ->padre). Necesario para que
	 // pulsar izq/der dentro de un submenu tambien marque options_changed_flag y el core
	 // refresque su estado (p.ej. cambio de paleta de video).
	 bool isCoreOptions(){
	   Menu* m = obtenerMenuActual();
	   while (m != NULL){
		 if (m == menuCoreOptions)
		   return true;
		 m = m->padre;
	   }
	   return false;
	 }



	bool isFrontendKeysMenu(){
		return obtenerMenuActual() == menuAssignFrontend;
	}

	bool isOverscanmenu(){
		return obtenerMenuActual() == menuOverscan;
	}

	CONFIG_STATUS getStatus(){ return status;}
	int getScrapGamesSelection(){return scrapGamesSelection;}
	
	void resetStatus(){
		status = NORMAL;
		OpcionLista* l = (OpcionLista*)menuAskSavestates->opciones[0];
		*l->indice = 0;
	}

	/* Coloca el cursor en newPos y recoloca la ventana visible en un solo
	 * paso.  Lo comparten navegar/nextPage/prevPage, que solo se diferencian
	 * en el destino que calculan.  Lleva dentro el guard de menuActual/status
	 * que antes vivia en navegar(). */
	void moveTo(int newPos);
	void nextPos();
    void prevPos();
	void nextPage();
	void prevPage();
	void volverMenuInicial();
	void poblarCdList(std::string ruta);
	
	Image imageFaq;
	TipoOpcion getMenuType(){
		if (menuActual->seleccionado < (int)menuActual->opciones.size()){
			return menuActual->opciones[menuActual->seleccionado]->tipo;
		} else {
			return OPC_UNDEFINED;
		}
	}
	
	std::string descargarLogros();
    static std::string sDescargarLogros(void* inst);
	static std::string changeHardcoreMode(void* inst, void *value);
	static std::string changeEnableAchievements(void* inst, void *value);
	static std::string sApplyCheats(void* inst, void *value);
	static std::string setDefaultEmu(void* inst, void *index, void *values);
	static std::string setControllerType(void* inst, void *index, void *values);
	static std::string cdromFileSelected(void* inst, void *value);
	static std::string cdromNextSelected(void* inst, void *value);
	static std::string cdromListAction(void* inst);
	static std::string bootWithoutDisk(void* inst, void *value);
	static std::string changeRAUser(void* inst, void *value);
	static std::string changeRAPassword(void* inst, void *value);
	static std::string changeRomPath(void* inst, void *value);
	static std::string changeScrapUser(void* inst, void *value);
	static std::string changeScrapPassword(void* inst, void *value);
	static std::string gameSearchAction(void* inst);
	static std::string gameGuidesSearchAction(void* inst, void *value);
	static std::string gameGuideAction(void* inst, void *value);
	static std::string selectBackground(void* inst, void *index, void *values);
	static std::string selectMusicVolume(void* inst, void *index, void *values);
	static std::string toggleMusicEnabled(void* inst, void *value);
	static std::string selectResolution(void* inst, void *index, void *values);

	static void onUserText(const std::string& text, void* userData);
	static void onScrapPasswordText(const std::string& text, void* userData);
	static void onRAUserText(const std::string& text, void* userData);
	static void onRAPasswordText(const std::string& text, void* userData);
	static void onRomPath(const std::string& text, void* userData);
};

template <typename T>
class OpcionExec : public Opcion {
public:
    // Definimos el puntero a un metodo de GestorMenus
    // Sintaxis: Tipo_Retorno (Nombre_Clase::*Nombre_Puntero)(Argumentos)
    typedef std::string (GestorMenus::*FuncType)(T*);
    
    FuncType execfunc;
    T* data;
    GestorMenus* instanciaGestor; // Necesitamos la instancia para llamar al metodo

    OpcionExec(std::string t, FuncType v, T* p, GestorMenus* gestor) 
        : Opcion(t, OPC_EXEC), execfunc(v), data(p), instanciaGestor(gestor) {}

	OpcionExec(std::string t, FuncType v, T* p, int ico, GestorMenus* gestor) 
        : Opcion(t, OPC_EXEC, ico), execfunc(v), data(p), instanciaGestor(gestor) {}

    // Implementacion del metodo virtual
    std::string ejecutar() {
        // En C++, para llamar a un puntero a funcion miembro:
        // (instancia.*puntero)(argumentos)
        return (instanciaGestor->*execfunc)(data);
    }
};

