#include <uiobjects/object.h>
#include <io/cfgloader.h>

#include <iostream>
#include <vector>
#include <string>

// --- Definición de tipos de opciones ---
enum TipoOpcion { OPC_BOOLEANA, OPC_LISTA, OPC_SUBMENU };

struct Menu; // Declaración anticipada

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

class OpcionLista : public Opcion {
public:
    std::vector<std::string> items;
    int* indice;
    OpcionLista(std::string t, std::vector<std::string> it, int* idx) 
        : Opcion(t, OPC_LISTA), items(it), indice(idx) {}
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

public:
    GestorMenus(int screenw, int screenh);
	~GestorMenus();
    // Inicializa la estructura de menús
    void inicializar(CfgLoader *refConfig);
    // Lógica de navegación Arriba/Abajo
    void navegar(int dir);
    // Lógica para cambiar valores (Izquierda / Derecha)
    void cambiarValor(int dir);
    // Lógica para confirmar (Botón A)
    void confirmar();
    // Lógica para volver (Botón B)
    void volver();
    // Método simple para obtener qué dibujar
    Menu* obtenerMenuActual();
	void draw(SDL_Surface *video_page);

	void nextPos();
    void prevPos();
};