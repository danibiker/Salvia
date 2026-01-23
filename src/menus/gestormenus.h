#include <uiobjects/object.h>
#include <io/cfgloader.h>
#include <io/joystick.h>
#include <beans/structures.h>

#include <iostream>
#include <vector>
#include <string>

// --- Definición de tipos de opciones ---
enum TipoOpcion { OPC_BOOLEANA, OPC_LISTA, OPC_SUBMENU, OPC_INT, OPC_KEY, OPC_EXEC};

enum TipoKey{
	KEY_JOY_BTN,
	KEY_JOY_HAT,
	KEY_JOY_AXIS
};

enum CONFIG_STATUS{
	NORMAL,
	POLLING_INPUTS,
	MAX_CONFIG_STATUS
};

struct Menu; // Declaración anticipada

//Interfaz para ejecutar funciones
typedef std::string (*ExecFunc)(Joystick* joy);

// Clase Base para las opciones del menú
class Opcion {
public:
    std::string titulo;
    TipoOpcion tipo;
    Opcion(std::string t, TipoOpcion tp) : titulo(t), tipo(tp) {}
    virtual ~Opcion() {}
};

class OpcionBool : public Opcion {
public:
    bool* valor;
    OpcionBool(std::string t, bool* v) : Opcion(t, OPC_BOOLEANA), valor(v) {}
};

class OpcionInt : public Opcion {
public:
    int* valor;
	std::string description;
    OpcionInt(std::string t, int* v, std::string desc) : Opcion(t, OPC_INT), valor(v), description(desc) {}
};

class OpcionExec : public Opcion {
public:
    ExecFunc execfunc;
	Joystick *joystick;
    OpcionExec(std::string t, ExecFunc v, Joystick *pjoystick) : Opcion(t, OPC_EXEC), execfunc(v), joystick(pjoystick) {}
};

class OpcionKey : public Opcion {
public:
    t_joy_retro_inputs* joyInputs;
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
	}
};

class OpcionLista : public Opcion {
public:
    std::vector<std::string> items;
    int* indice;

    OpcionLista(std::string t, std::vector<std::string> it, int* idx) 
        : Opcion(t, OPC_LISTA), items(it), indice(idx)  {}
};

class OpcionSubMenu : public Opcion {
public:
    Menu* destino;
    OpcionSubMenu(std::string t, Menu* d) : Opcion(t, OPC_SUBMENU), destino(d) {}
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

	int getScreenNumLines();
	void resetIndexPos();
	void clearSelectedText();
	void setLayout(int layout, int screenw, int screenh);
	void addControlerOptions(Menu*&, int, Joystick *, CfgLoader *);
	void addControlerButtons(Menu*&, int);
	int findAxisPos(int retroDirection);
	void resetKeyElement(int, TipoKey);

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
    std::string confirmar();
    // Lógica para volver (Botón B)
    void volver();
    // Método simple para obtener qué dibujar
    Menu* obtenerMenuActual();
	void draw(SDL_Surface *video_page);
	void updateButton(int);
	void updateAxis(int, int);
	bool options_changed_flag;

	void poblarCoreOptions(CfgLoader *);

	bool isCoreOptions(){
		return obtenerMenuActual() == menuCoreOptions;
	}

	CONFIG_STATUS getStatus(){ return status;}

	void nextPos();
    void prevPos();
};