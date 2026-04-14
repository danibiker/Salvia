#define NOMINMAX
#include <uiobjects/object.h>
#include <io/cfgloader.h>
#include <io/joystick.h>
#include <io/dirutil.h>
#include <uiobjects/image.h>
#include <beans/structures.h>
#include <font/fonts.h>
#include <http/badgedownloader.h>

#include <iostream>
#include <vector>
#include <string>

// --- Definición de tipos de opciones ---
enum TipoOpcion { OPC_BOOLEANA, OPC_LISTA, OPC_SUBMENU, OPC_INT, OPC_KEY, OPC_EXEC, OPC_SHOW_TXT, OPC_SHOW_TXT_VAL, OPC_SAVESTATE, OPC_ACHIEVEMENT};
enum TipoKey{KEY_JOY_BTN,KEY_JOY_HAT,KEY_JOY_AXIS, KEY_JOY_MAX};
enum ACTION_ASK{ASK_CARGAR, ASK_GUARDAR, ASK_ELIMINAR, MAX_ASK};
enum CONFIG_STATUS{NORMAL,POLLING_INPUTS,ASK_SAVESTATES, EXIT_CONFIG, EXIT_EMULATION, START_SCRAPPING, MAX_CONFIG_STATUS};


struct t_option_action{
	int option;
	int action;
	void *elem;
	int indexSelected;
	std::string message;
	
	t_option_action(){
		option = 0;
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


struct Menu; // Declaración anticipada
class OpcionSavestate;
class OpcionBool;

// Definimos un tipo de función que devuelve string y no recibe nada
typedef std::string (*GestorCallback)(void* instance);
typedef std::string (*CallbackValue) (void* instance, void* value);
typedef std::string (*CallbackValues)(void* instance, void* index, void* values);

// Clase Base para las opciones del menú
class Opcion {
public:
    std::string titulo;
    TipoOpcion tipo;
	int icon;
    Opcion(std::string t, TipoOpcion tp) : titulo(t), tipo(tp), icon(-1) {}
	Opcion(std::string t, TipoOpcion tp, int ico) : titulo(t), tipo(tp), icon(ico) {}
	virtual std::string ejecutar() = 0; // Método virtual puro
    virtual ~Opcion() {}
};

//Esta clase no permite modificar ningún valor, sólo muestra texto
class OpcionTxt : public Opcion {
public:
    OpcionTxt(std::string t) : Opcion(t, OPC_SHOW_TXT) {}
	std::string ejecutar() override {
        return "";
    }
};

//Esta clase no permite modificar ningún valor, sólo muestra texto y un valor
class OpcionTxtAndValue : public Opcion {
public:
	std::string valor;
    OpcionTxtAndValue(std::string t, std::string v) : Opcion(t, OPC_SHOW_TXT_VAL), valor(v) {}
	std::string ejecutar() override {
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
	CallbackValue callback; // Función estática
    void* context;          // El "this" de GestorMenus

    OpcionBool(std::string t, bool* v) : Opcion(t, OPC_BOOLEANA), valor(v), callback(NULL), context(NULL) {}

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
    OpcionInt(std::string t, int* v, std::string desc) : Opcion(t, OPC_INT), valor(v), description(desc) {}
	std::string ejecutar() override {
        return "";
    }
};

class OpcionLista : public Opcion {
public:
    std::vector<std::string> items;
    int* indice;
	CallbackValues callback; // Función estática
    void* context;

    OpcionLista(std::string t, std::vector<std::string> it, int* idx) 
        : Opcion(t, OPC_LISTA), items(it), indice(idx), callback(NULL), context(NULL) {}
	
	std::string ejecutar() override {
        if (callback != NULL && indice != NULL) {
            return callback(context, (void *)indice, (void*)&items); 
        }
        return "";
    }
};

class OpcionSubMenu : public Opcion {
public:
    Menu* destino;
	GestorCallback callback; // Función estática
    void* context;           // El "this" de GestorMenus

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

	OpcionKey(std::string t, t_joy_state *pjoyInputs, t_joy_mapper * pjoyMapper, int pgamepadId, int pBtn, TipoKey ptipoKey, std::string desc): Opcion(t, OPC_KEY){
		btn = pBtn;
		gamepadId = pgamepadId;
		tipoKey = ptipoKey;
		description = desc;
		lastTimeAsked = 0;
		changeAsked = false;
		joyInputs = pjoyInputs;
		joyMapper = pjoyMapper;
		intRef = NULL;
	}
	
	std::string ejecutar() override {
        return "";
    }
};

// Estructura del Menú
struct Menu{
    std::string titulo;
    std::vector<Opcion*> opciones;
    int seleccionado;
    Menu* padre;
	int rowHeight;
	int menuWidth;

    Menu(std::string t, Menu* p = NULL) : titulo(t), seleccionado(0), padre(p) {
		TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
		rowHeight = TTF_FontLineSkip(fontMenu);
	}
	
	Menu(std::string t, int rh, int mw, Menu* p = NULL) : titulo(t), seleccionado(0), padre(p) {
		rowHeight = rh;
		menuWidth = mw;
	}

    ~Menu() {
        for(std::size_t i = 0; i < opciones.size(); i++) delete opciones[i];
    }
};


// --- Clase Principal de Gestión de Menús ---
class GestorMenus : public Object{
private:
    Menu* menuRaiz;     // Menú principal (almacenado permanentemente)
    Menu* menuActual;   // Puntero al menú que se está mostrando ahora
    static const int waitTitleMove = 2000;
	static const int textFps = 20;
	static const int frameTimeText = (int)(1000 / textFps);

    // Lista de todos los menús para liberar memoria al final
    std::vector<Menu*> todosLosMenus;
	Menu* menuCoreOptions;
	Menu* menuSavestates;
	Menu* menuAskSavestates;
	Menu* menuScrapper;
	Menu* menuAchievements;
	Menu* menuAssignRetro;
	int askNumOptions;

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
    bool keyUp;
    int lastSel;
    float pixelShift;
    static SDL_Surface* imgText;
	std::string lastImagePath;
	Image imageMenu;
	int scrapGamesSelection;

	int getScreenNumLines();
	void clearSelectedText();
	void setLayout(int layout, int screenw, int screenh);
	void addControlerOptions(Menu*&, int, Joystick *, CfgLoader *);
	void addControlerButtons(Menu*&, int, Joystick *);
	int findAxisPos(int retroDirection);
	void resetKeyElement(int, TipoKey);
	void drawSelectionBox(int i, SDL_Surface *video_page, SDL_Color& lineTextColor);
	void drawSavestateWithImage(int, OpcionSavestate *, SDL_Surface *);
	void drawBooleanSwitch(int, OpcionBool *, SDL_Surface *);
	void drawAskMenu(SDL_Surface *video_page);
	void drawKeys(int i, OpcionKey *opt, SDL_Surface *video_page);
	void drawAchievement(int, OpcionAchievement *, SDL_Surface *);
	void resetAskPosition();
	void poblarMenuScrapper(CfgLoader *refConfig, Menu* menuScrapper);
	void poblarMenuHotkeys(Menu* menuHotkeys, Joystick *joystick);
	void poblarMenuAssignFrontend(Menu* menuHotkeys, Joystick *joystick);
	std::string guardarJoysticks(Joystick* joy);
	std::string guardarGameJoysticks(Joystick* joy);
	std::string guardarCoreJoysticks(Joystick* joy);
	std::string guardarCoreConfig(CfgLoader *refConfig);
	std::string guardarMainConfig(CfgLoader *refConfig);
	std::string volverEmulacion(CONFIG_STATUS *st);
	std::string salirEmulacion(CONFIG_STATUS *st);
	std::string startScrapping(CONFIG_STATUS *st);
	

public:
    GestorMenus(int screenw, int screenh);
	~GestorMenus();
    // Inicializa la estructura de menús
    void inicializar(CfgLoader *, Joystick *);
    // Lógica de navegación Arriba/Abajo
    void navegar(int dir);
    // Lógica para cambiar valores (Izquierda / Derecha)
    void cambiarValor(int dir);
    // Lógica para confirmar (Botón A)
    std::string confirmar(t_option_action *);
    // Lógica para volver (Botón B)
    void volver();
	void resetIndexPos();
    // Método simple para obtener qué dibujar
    Menu* obtenerMenuActual();
	void setAchievementsAsSelected(){menuActual = menuAchievements;}
	void draw(SDL_Surface *video_page);
	void updateButton(int, TipoKey);
	void updateAxis(int, int);
	bool options_changed_flag;
	std::vector<t_scrap> scrapSelection;
	void poblarCoreOptions(CfgLoader *);
	void poblarPartidasGuardadas(CfgLoader *, std::string);
	void poblarJoystickTypes(Joystick *joystick);

	std::string stopScrapping(CONFIG_STATUS *st);
	void loadAchievements();

	bool isCoreOptions(){
		return obtenerMenuActual() == menuCoreOptions;
	}

	CONFIG_STATUS getStatus(){ return status;}
	int getScrapGamesSelection(){return scrapGamesSelection;}
	
	void resetStatus(){
		status = NORMAL;
		OpcionLista* l = (OpcionLista*)menuAskSavestates->opciones[0];
		*l->indice = 0;
	}

	void nextPos();
    void prevPos();
	void volverMenuInicial();
	
    std::string descargarLogros();
    static std::string sDescargarLogros(void* inst);
	static std::string changeHardcoreMode(void* inst, void *value);
	static std::string setDefaultEmu(void* inst, void *index, void *values);
	static std::string setControllerType(void* inst, void *index, void *values);
};

template <typename T>
class OpcionExec : public Opcion {
public:
    // Definimos el puntero a un método de GestorMenus
    // Sintaxis: Tipo_Retorno (Nombre_Clase::*Nombre_Puntero)(Argumentos)
    typedef std::string (GestorMenus::*FuncType)(T*);
    
    FuncType execfunc;
    T* data;
    GestorMenus* instanciaGestor; // Necesitamos la instancia para llamar al método

    OpcionExec(std::string t, FuncType v, T* p, GestorMenus* gestor) 
        : Opcion(t, OPC_EXEC), execfunc(v), data(p), instanciaGestor(gestor) {}

	OpcionExec(std::string t, FuncType v, T* p, int ico, GestorMenus* gestor) 
        : Opcion(t, OPC_EXEC, ico), execfunc(v), data(p), instanciaGestor(gestor) {}

    // Implementación del método virtual
    std::string ejecutar() {
        // En C++, para llamar a un puntero a función miembro:
        // (instancia.*puntero)(argumentos)
        return (instanciaGestor->*execfunc)(data);
    }
};