#define NOMINMAX
#include <uiobjects/object.h>
#include <io/cfgloader.h>
#include <io/joystick.h>
#include <io/dirutil.h>
#include <uiobjects/image.h>
#include <beans/structures.h>

#include <iostream>
#include <vector>
#include <string>

// --- Definición de tipos de opciones ---
enum TipoOpcion { OPC_BOOLEANA, OPC_LISTA, OPC_SUBMENU, OPC_INT, OPC_KEY, OPC_EXEC, OPC_SHOW_TXT, OPC_SHOW_TXT_VAL, OPC_SAVESTATE};

enum TipoKey{KEY_JOY_BTN,KEY_JOY_HAT,KEY_JOY_AXIS};
static const char *TipoKeyStr[] = {"Btn: ", "Hat: ", "Axis: "};

enum ACTION_ASK{ASK_CARGAR, ASK_GUARDAR, ASK_ELIMINAR, MAX_ASK};
static const char *ACTION_ASK_STR[] = {"Cargar", "Guardar", "Eliminar"};

enum CONFIG_STATUS{NORMAL,POLLING_INPUTS,ASK_SAVESTATES, EXIT_CONFIG, MAX_CONFIG_STATUS};

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


struct Menu; // Declaración anticipada
class OpcionSavestate;
class OpcionBool;

// Clase Base para las opciones del menú
class Opcion {
public:
    std::string titulo;
    TipoOpcion tipo;
    Opcion(std::string t, TipoOpcion tp) : titulo(t), tipo(tp) {}
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
    OpcionBool(std::string t, bool* v) : Opcion(t, OPC_BOOLEANA), valor(v) {}
	std::string ejecutar() override {
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

    OpcionLista(std::string t, std::vector<std::string> it, int* idx) 
        : Opcion(t, OPC_LISTA), items(it), indice(idx)  {}
	
	std::string ejecutar() override {
        return "";
    }
};

class OpcionSubMenu : public Opcion {
public:
    Menu* destino;
    OpcionSubMenu(std::string t, Menu* d) : Opcion(t, OPC_SUBMENU), destino(d) {}
	std::string ejecutar() override {
        return "";
    }
};

template <typename T>
class OpcionExec : public Opcion {
public:
    // Definimos el puntero a función que acepta el tipo específico T
    typedef std::string (*FuncType)(T*);
    
    FuncType execfunc;
    T* data;

    OpcionExec(std::string t, FuncType v, T* p) 
        : Opcion(t, OPC_EXEC), execfunc(v), data(p) {}

    // Implementación del método virtual
    std::string ejecutar() override {
        return execfunc(data);
    }
};

class OpcionKey : public Opcion {
public:
    t_joy_retro_inputs* joyInputs;
	int *intRef; 

	//En idx tenemos el indice del array en el que se guardo el boton
	int idx;
	//En btn tenemos el boton de libretro asignado: RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B, ...
	int btn;
	int gamepadId;

	std::string description;
	bool changeAsked; 
	Uint32 lastTimeAsked;
	TipoKey tipoKey;

	OpcionKey(std::string t, t_joy_retro_inputs (&listaMapeos)[MAX_PLAYERS], int pgamepadId, int pIdx, int pBtn, TipoKey ptipoKey, std::string desc): Opcion(t, OPC_KEY){
		idx = pIdx;
		btn = pBtn;
		gamepadId = pgamepadId;
		tipoKey = ptipoKey;
		description = desc;
		lastTimeAsked = 0;
		changeAsked = false;
		joyInputs = &listaMapeos[pgamepadId];
		intRef = NULL;
	}
	
	//En este caso no se necesita conversion
	OpcionKey(std::string t, int* pintRef, TipoKey ptipoKey, std::string desc): Opcion(t, OPC_KEY){
		joyInputs = NULL;
		tipoKey = ptipoKey;
		description = desc;
		lastTimeAsked = 0;
		changeAsked = false;
		intRef = pintRef;
		idx = *pintRef;
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

    Menu(std::string t, Menu* p = NULL) : titulo(t), seleccionado(0), padre(p) {}
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

	int getScreenNumLines();
	void resetIndexPos();
	void clearSelectedText();
	void setLayout(int layout, int screenw, int screenh);
	void addControlerOptions(Menu*&, int, Joystick *, CfgLoader *);
	void addControlerButtons(Menu*&, int);
	int findAxisPos(int retroDirection);
	void resetKeyElement(int, TipoKey);
	
	void drawSavestateWithImage(int, OpcionSavestate *, SDL_Surface *);
	void drawBooleanSwitch(int, OpcionBool *, SDL_Surface *);
	void drawAskMenu(SDL_Surface *video_page);

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
    // Método simple para obtener qué dibujar
    Menu* obtenerMenuActual();
	void draw(SDL_Surface *video_page);
	void updateButton(int, TipoKey);
	void updateAxis(int, int);
	bool options_changed_flag;

	void poblarCoreOptions(CfgLoader *);
	void poblarMenuHotkeys(Menu* menuHotkeys, Joystick *joystick);
	void poblarPartidasGuardadas(CfgLoader *, std::string);

	bool isCoreOptions(){
		return obtenerMenuActual() == menuCoreOptions;
	}

	CONFIG_STATUS getStatus(){ 
		return status;
	}
	
	void resetStatus(){
		status = NORMAL;
		OpcionLista* l = (OpcionLista*)menuAskSavestates->opciones[0];
		*l->indice = 0;
	}

	

	void nextPos();
    void prevPos();
};