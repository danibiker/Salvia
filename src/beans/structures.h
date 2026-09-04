#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdint.h>

#include <const/constant.h>
#include <const/menuconst.h>
#include <const/keyconst.h>
#include <unzip/ZipBrowser.h>
#include <io/fileprops.h>



class FileLaunch{
    public:
            FileLaunch(){};
            ~FileLaunch(){};
            std::string nombreemu;
            std::string rutaexe;
            std::string fileexe;
            std::string parmsexe;
            std::string rutaroms;
            std::string nombrerom;
            std::string titlerom;
            std::string nplayers;
            std::string categ;
            bool descomprimir;
            bool fixoption;
};

class Executable{
    public:
        Executable(){
            ejecutable = "";
            param = "";
            filerompath = "";
            comandoFinal = "";
            filenameinparms = false;
        }
        ~Executable(){}
        std::string ejecutable;
        std::string param;
        std::string filerompath;
        std::string comandoFinal;
        bool filenameinparms;
};

struct GameTicks{
    uint32_t ticks;
	double lastTime;
	float dt;

	GameTicks(){
		ticks = 0;
		lastTime = 0;
		dt = 0.0;
	}
};

struct Dimension{
    int w, h;
};

struct ListStatus{
    int emuLoaded;
    int iniPos;
    int endPos;
    int curPos;
    int maxLines;
    int layout;
    bool animateBkg;
	int posManufacturer;
	int posSystem;
	int posYear;
	bool onlyParents;
	//Es el nombre del zip abierto
	char zipname[260];
	//Es el path seleccionado dentro del zip
	char zippedPath[4096];
	//Relative path selected
	char relativePath[4096];

	ListStatus(){
		std::memset(relativePath, 0, sizeof(relativePath));
		std::memset(zippedPath, 0, sizeof(zippedPath));
		std::memset(zipname, 0, sizeof(zipname));
	}

	// Constructor con parametros de entrada
    ListStatus(int _emuLoaded, int _iniPos, int _endPos, int _curPos, int _maxLines, 
               int _layout, bool _animateBkg, int _posManufacturer, int _posSystem, 
               int _posYear, bool _onlyParents)
        : emuLoaded(_emuLoaded),
          iniPos(_iniPos),
          endPos(_endPos),
          curPos(_curPos),
          maxLines(_maxLines),
          layout(_layout),
          animateBkg(_animateBkg),
          posManufacturer(_posManufacturer),
          posSystem(_posSystem),
          posYear(_posYear),
          onlyParents(_onlyParents)
    {
        // Los arrays de char se siguen inicializando vac�os autom�ticamente
        std::memset(relativePath, 0, sizeof(relativePath));
        std::memset(zippedPath, 0, sizeof(zippedPath));
        std::memset(zipname, 0, sizeof(zipname));
    }    
	
};

struct t_scale_props{
	// 1. Punteros (4 u 8 bytes dependiendo de la arquitectura)
    uint16_t* src; 
    uint16_t* dst; 

    // 2. Tipos de 8 bytes (size_t en 64 bits o si usas uint64_t)
    std::size_t spitch;
    std::size_t dpitch; 

    // 3. Tipos de 4 bytes (int y float)
    int sw; 
    int sh; 
    int dw; 
    int dh; 
    int scale; 
    float ratio;
	int bpp;
	int filter;
	bool integer_scale;
	int integer_scale_type;

	t_scale_props(){
		sw = sh = dw = dh = scale = 0;
		ratio = .0f;
		bpp = 16;
		integer_scale = false;
		integer_scale_type = SCALE_INT_REDUCE;
	}
};

// Buffer para alojar el resultado de Scale2x (ej. 320x224 -> 640x448)
// Reservamos para el caso maximo (ej. 512x512 -> 1024x1024)
// 2048 * 1152 permite hasta Scale4x de una imagen de 512x256 o xBRZ alto
//
// ★ NO convertir esto en 'extern' con una definicion unica, por tentador que sea.
// Al ser 'static' en una cabecera, CADA unidad de compilacion tiene su propia copia,
// y de eso depende que funcione: los dos consumidores reales son menus/gamemenu.cpp
// (hilo principal) y menus/menuassetloader.cpp, que es un HILO PERSISTENTE aparte
// (CreateThread, pineado a IO_THREAD en la 360). Compartir un unico scratch entre los
// dos haria que el escalado de una imagen del cargador de assets y el del frame se
// pisaran. La separacion es lo que los protege.
//
// El precio es que las ~16 unidades que incluyen structures.h y NO escalan nada se
// llevan igualmente 4,5 MB de BSS cada una. Si algun dia molesta, la forma correcta de
// arreglarlo es mover el buffer a quien escala y darle uno POR HILO (o por objeto),
// no unificarlo.
//
// Riesgo aparte, sin resolver: 2048*1152 no cubre scale4x de una fuente de 512x384
// (1536 lineas de salida). Con una fuente asi se sale del array.
static uint16_t temp_buffer[2048 * 1152]; // Ocupa aprox 4.5 MB

struct GameDataFields{
private:
	int lastYear;
	int lastManufacturer;
	int lastSystem;
	bool lastOnlyParents;

public:
    std::vector<std::string> years;
    std::vector<std::string> manufacturers;
    std::vector<std::string> systems;

	int posYear;
	int posManufacturer;
	int posSystem;
	bool onlyParents;
	

	GameDataFields(){
		clear();
		//By default, show only the parent roms
		onlyParents = true;
	}

	void clear(){
		resetFilters();
		years.clear();
		manufacturers.clear();
		systems.clear();
	}

	void resetFilters(){
		posYear = -1;
		posManufacturer = -1;
		posSystem = -1;
		lastYear = -1;
		lastManufacturer = -1;
		lastSystem = -1;
		lastOnlyParents = onlyParents;
	}
	
	//We call here usually when we don't have data. So if there is nothing selected,
	//we include it by default
	bool shouldInclude(){
		return posYear == -1 && posManufacturer == -1 && posSystem == -1;
	}

	bool filterChanged(){
		if (posYear != lastYear || posManufacturer != lastManufacturer || posSystem != lastSystem || onlyParents != lastOnlyParents){
			lastYear = posYear;
			lastManufacturer = posManufacturer;
			lastSystem = posSystem;
			lastOnlyParents = onlyParents;
			return true;
		} 
		return false;
	} 

	bool filterManufacturer(const std::string &manufacturer){
		return posManufacturer == -1 || (posManufacturer >= -1 && posManufacturer < (int)manufacturers.size() && manufacturer.find(manufacturers[posManufacturer]) != std::string::npos);
	}
	
	bool filterYear(const std::string &year){
		return posYear == -1 || (posYear >= -1 && posYear < (int)years.size() && year.find(years[posYear]) != std::string::npos);
	}

	bool filterSystem(const std::string &system){
		return posSystem == -1 || (posSystem >= -1 && posSystem < (int)systems.size() && system.find(systems[posSystem]) != std::string::npos);
	}

	bool filterParent(const bool &parent) {
		return !onlyParents || parent;
	}
};

struct GameData {
	uint16_t year;
	// Si esta vacio, es un "parent" (original)
    char cloneof[25];  
    char romof[25];
	char sourcefile[25];
    char manufacturer[50];
	char description[80];

	GameData(){
		cloneof[0] = '\0';
		romof[0] = '\0';
		sourcefile[0] = '\0';
		manufacturer[0] = '\0';
		description[0] = '\0';
	}

	// --- M�todos SET seguros ---
    void setCloneof(const char* src) {
        // Copia asegurando que no se pase del tama�o y fuerza el car�cter nulo al final
        std::strncpy(cloneof, src, sizeof(cloneof) - 1);
        cloneof[sizeof(cloneof) - 1] = '\0';
    }

    void setRomof(const char* src) {
        std::strncpy(romof, src, sizeof(romof) - 1);
        romof[sizeof(romof) - 1] = '\0';
    }

    void setSourcefile(const char* src) {
        std::strncpy(sourcefile, src, sizeof(sourcefile) - 1);
        sourcefile[sizeof(sourcefile) - 1] = '\0';
    }

    void setManufacturer(const char* src) {
        std::strncpy(manufacturer, src, sizeof(manufacturer) - 1);
        manufacturer[sizeof(manufacturer) - 1] = '\0';
    }

	void setDescription(const char* src){
		std::strncpy(description, src, sizeof(description) - 1);
        description[sizeof(description) - 1] = '\0';
	}

    // util para saber si es clon rapidamente
    bool isClone() const { return *cloneof != '\0'; }
	bool isSystem() const { return *cloneof != '\0' && *romof != '\0'; }
};

enum FILE_TYPE
{
	FT_DIR = 0, 
    FT_ZIP_LIST,
	FT_CARTRIDGE
};

class GameFile{
public:
	// id de consola
	int systemid;
	// Path del fichero
    std::string longFileName;
	// Nombre del titulo
    std::string gameTitle;
	// Nombre en minusculas pre-calculado para ordenar
	std::string sortKey; 
	// Superficie para cachear textos
	SDL_Surface *cache;
	//Detalle del juego
	const GameData *gameData;
	//Indica si es un directorio
	FILE_TYPE fileType;

	GameFile() : 
        systemid(0), 
        cache(NULL), 
        gameData(NULL), 
        fileType(FT_CARTRIDGE) 
    {
    }

    ~GameFile(){
		if (cache != NULL){
			SDL_FreeSurface(cache);
			cache = NULL;
		}
    }
};

class FileName8_3 {
    std::string shortFN;
    std::string longFN;

    FileName8_3(std::string shortFN, std::string longFN) {
        this->shortFN = shortFN;
        this->longFN = longFN;
    }
};

class ConfigMain{
    public:
		ConfigMain(){
			resolution[0] = 0;
			resolution[1] = 0;
			debug = false;
			path_prefix = "";
			alsaReset = false;
			background_music = 0;
			mp3_file = "";
			aspectRatio = RATIO_CORE;
			scaleMode = FULLSCREEN;
			syncMode = OPT_SYNC_VIDEO;
			sonidoMode = 1;
		}

		~ConfigMain(){}

		std::vector<std::string> emulators;
		bool debug;
		std::string path_prefix;
		int resolution[2];
		bool alsaReset;
		int background_music;
		std::string mp3_file;
		
		
		int scaleMode;
		int aspectRatio;
		int syncMode;
		bool sonidoMode;
	
};

struct t_joy_inputs{
	int *buttons;
	int *axis;
	int *hats;
	std::string joyName;

	int nButtons;
	int nAxis;
	int nHats;
};

struct t_retro_input{
	int joy;
	int key;

	void setJoy(int i){
		joy = i;
	}

	t_retro_input(){
		joy = -1;
		key = -1;
	}
};

struct t_key_input{
    int key;
    int keyMod;
    int unicode;
    bool keyjoydown;

	t_key_input() {
		key = -1;
		keyMod = -1;
		unicode = -1;
		keyjoydown = false;
	}
};

/* Aqui habia MAX_BUTTONS == MAX_AXIS == MAX_HATS == MAXJOYBUTTONS, los tres iguales
 * POR ACCIDENTE: MAXJOYBUTTONS es el tamano del enum joystickButtons, que no tiene
 * nada que ver con lo que el mando reporta. De esa confusion salieron la guarda
 * "axis >= MAX_AXIS/2" (cuyo comentario admitia que la version anterior escribia fuera
 * del array) y que hats_state usara 4 de 31 posiciones.
 *
 * Son DOS espacios distintos, y cada tabla del mapper tiene un pie en cada uno:
 *   - el ESPACIO SDL, que es lo que entrega el mando: numero de boton, mascara de hat,
 *     o direccion de eje (eje*2 + signo). Indexa sdlToX[] y los arrays de estado.
 *   - el ESPACIO DE DESTINOS, que son ids logicos: RETRO_DEVICE_ID_JOYPAD_* (0..15),
 *     JOY_BUTTON_* JOY_AXIS* del frontend (0..30) y HK_* de las hotkeys (0..10).
 *     Indexa XToSdl[] y rapidFire[].
 */
#define MAX_TARGETS         MAXJOYBUTTONS   /* ids logicos: el mayor es JOY_AXIS_R2 */
#define MAX_SDL_BUTTONS     24              /* botones que puede reportar SDL       */
#define MAX_SDL_HAT_VALUES  16              /* mascara de hat: 0..15, diagonales incluidas */
#define MAX_ANALOG_AXIS     16
#define MAX_SDL_AXIS_DIRS   (MAX_ANALOG_AXIS * 2)  /* eje*2 + signo */

/* Eje que comparten LT y RT. Solo lo usa Windows: en la 360 los gatillos son botones
 * digitales, y ese eje 2 es en realidad la X del stick derecho.
 * (Aqui habia tambien AXIS_LT/AXIS_RT, dos "slots libres" de g_analog_state que no
 * escribia nadie; su unica lectura, en salvia.cpp, devolvia siempre 0.) */
static const int XBOX_COMBINED_TRIGGER_AXIS = 2;

/* --- Remapeo de las direcciones analogicas -----------------------------------
 * Ocho SLOTS, uno por cada direccion con nombre de los dos sticks, en el orden
 * del enum joystickButtons (constant.h): JOY_AXIS1_RIGHT, _LEFT, _UP, _DOWN y
 * luego los cuatro JOY_AXIS2_*.
 *
 * El slot es la direccion FISICA del stick, y el valor guardado es EN QUE SE
 * CONVIERTE. Al reves que las otras tres tablas del mapper, que van de fisico a
 * id del core. Esto es lo que permite que funcione con cualquier core: si una
 * direccion se convierte en un boton, se enciende su btn_state y a partir de ahi
 * es indistinguible de una pulsacion de verdad, asi que pasa por mapperCore como
 * el resto. Sustituye al viejo axisAsPad, que era un interruptor global por
 * jugador y solo sabia hacer "todos los ejes como cruceta".
 *
 * Valor, codificado en un solo entero para que quepa en una clave del .joy:
 *
 *   -1                          sin asignar (esa direccion no hace nada)
 *   0 .. 2*MAX_ANALOG_AXIS-1    otra direccion de eje, en el espacio virtual
 *                               eje*2 + (valor > 0). El core recibe el ANALOGICO,
 *                               ya intercambiado entre sticks o invertido.
 *   ANALOG_DST_BTN_BASE + n     enciende el boton SDL n al cruzar la deadzone
 *   ANALOG_DST_HAT_BASE + h     enciende la posicion de hat h (mascara SDL)
 * -------------------------------------------------------------------------- */
#define ANALOG_TARGETS      8
#define ANALOG_DST_BTN_BASE 1000
#define ANALOG_DST_HAT_BASE 2000

/* Que direccion fisica (indice virtual eje*2+signo) es cada slot. Es una
 * DESCRIPCION DE LA PLATAFORMA, no una preferencia: el stick derecho no usa los
 * mismos ejes SDL en las dos. En la 360 son el 2(X) y el 3(Y); en Windows el
 * 4(X) y el 3(Y), porque alli el eje 2 son los gatillos combinados. */
#ifdef _XBOX
static const int analogSlotAxis[ANALOG_TARGETS] = {
	1, 0, 2, 3,   /* izq: eje 0 = X (0 izq / 1 der), eje 1 = Y (2 arr / 3 abj) */
	5, 4, 6, 7    /* der: eje 2 = X (4 izq / 5 der), eje 3 = Y (6 arr / 7 abj) */
};
#else
static const int analogSlotAxis[ANALOG_TARGETS] = {
	1, 0, 2, 3,   /* izq: igual en las dos plataformas                        */
	9, 8, 6, 7    /* der: eje 4 = X (8 izq / 9 der), eje 3 = Y (6 arr / 7 abj) */
};
#endif


/* --------------------------------------------------------------------------
 * INDICE SDL DE CADA BOTON LOGICO
 *
 * Indexada por JOY_BUTTON_* (A=0 .. R3=9), da el numero de boton que ENTREGA
 * SDL para ese boton fisico. Es el gemelo de analogSlotAxis de arriba, y por el
 * mismo motivo: DESCRIPCION DE LA PLATAFORMA, no una preferencia.
 *
 * En Windows es la identidad; en la 360 (SDL de Lantus) los cuatro del medio
 * van al reves: L3/R3 ocupan el 6 y el 7, y START/SELECT el 8 y el 9.
 *
 * Existe porque los defaults se escribian usando JOY_BUTTON_* COMO si fuera el
 * indice SDL (configMapperRetro y el constructor de Hotkeys), y eso solo vale en
 * Windows: en la 360 salian START, SELECT, L3 y R3 cruzados, tanto en el mapeo
 * del mando como en las hotkeys.
 * -------------------------------------------------------------------------- */
#ifdef _XBOX
static const int sdlIndexOfLogicalBtn[] = {
	0, 1, 2, 3,   /* A, B, X, Y     */
	4, 5,         /* L, R           */
	9, 8,         /* SELECT, START  <- cruzados respecto a Windows */
	6, 7          /* L3, R3         <- idem */
};
#else
static const int sdlIndexOfLogicalBtn[] = {
	0, 1, 2, 3,   /* A, B, X, Y     */
	4, 5,         /* L, R           */
	6, 7,         /* SELECT, START  */
	8, 9          /* L3, R3         */
};
#endif

/* Traduce un JOY_BUTTON_* a indice SDL. Fuera de rango devuelve el propio id,
 * que es el comportamiento de antes.
 * 'static inline' y no 'static' a secas: siendo una cabecera que incluye medio
 * proyecto, la version static suelta un C4505 (funcion sin usar) en cada unidad
 * que no la llame -- el mismo motivo por el que analogSlotOfVirtual acabo siendo
 * miembro de t_joy_mapper. */
static inline int sdlBtnOf(int logicalBtn){
	const int n = (int)(sizeof(sdlIndexOfLogicalBtn) / sizeof(sdlIndexOfLogicalBtn[0]));
	if (logicalBtn < 0 || logicalBtn >= n) return logicalBtn;
	return sdlIndexOfLogicalBtn[logicalBtn];
}

/* --------------------------------------------------------------------------
 * DEADZONE POR MANDO
 *
 * Umbral a partir del cual una direccion de stick cuenta como pulsada al
 * convertirla en boton o en cruceta (ver analogDst arriba y el manejador de
 * SDL_JOYAXISMOTION en io/joystick.cpp).  Antes era la constante global
 * DEADZONE = 10000 de const/constant.h, igual para todos los mandos.
 *
 * Se guarda el INDICE en esta tabla, no el valor: es lo que espera el widget
 * OpcionLista del menu y lo que hace la clave 'dz=' del .joy estable si algun
 * dia se anaden valores intermedios... siempre que se anadan AL FINAL.  Si se
 * inserta uno en medio, los .joy ya guardados apuntaran a otro valor.
 * -------------------------------------------------------------------------- */
#define DEADZONE_STEPS 7
static const int deadzoneValues[DEADZONE_STEPS] = {
	1000, 2000, 5000, 10000, 15000, 20000, 25000
};
/* Indice del 5000 */
#define DEADZONE_DEFAULT_IDX 2

struct t_joy_mapper{
	/* SDL -> destino: indexadas por lo que entrega el mando */
	int sdlToHat[MAX_PLAYERS][MAX_SDL_HAT_VALUES];
	int sdlToAxis[MAX_PLAYERS][MAX_SDL_AXIS_DIRS];
	int sdlToBtn[MAX_PLAYERS][MAX_SDL_BUTTONS];

	/* destino -> SDL: indexadas por id logico */
	int hatToSdl[MAX_PLAYERS][MAX_TARGETS];
	int axisToSdl[MAX_PLAYERS][MAX_TARGETS];
	int btnToSdl[MAX_PLAYERS][MAX_TARGETS];

	/* Direccion fisica del stick -> en que se convierte. Ver ANALOG_DST_BTN_BASE. */
	int analogDst[MAX_PLAYERS][ANALOG_TARGETS];

	/* Indice en deadzoneValues[], uno por jugador. Lo apunta directamente el
	 * OpcionLista del menu de asignacion, de ahi que sea int y no un enum. */
	int deadzoneIdx[MAX_PLAYERS];

	t_joy_mapper(){
		clear(sdlToHat, -1);
		clear(sdlToAxis, -1);
		clear(sdlToBtn, -1);

		clear(hatToSdl, -1);
		clear(axisToSdl, -1);
		clear(btnToSdl, -1);

		clear(analogDst, -1);

		for (int i = 0; i < MAX_PLAYERS; i++){
			deadzoneIdx[i] = DEADZONE_DEFAULT_IDX;
		}
	}

	/* Umbral en unidades de eje para este jugador. Acota el indice en vez de
	 * fiarse de el: llega de un fichero editable a mano. */
	int getDeadzone(int player){
		int idx;
		if (player < 0 || player >= MAX_PLAYERS) return deadzoneValues[DEADZONE_DEFAULT_IDX];
		idx = deadzoneIdx[player];
		if (idx < 0 || idx >= DEADZONE_STEPS) idx = DEADZONE_DEFAULT_IDX;
		return deadzoneValues[idx];
	}

	void setDeadzoneIdx(int player, int idx){
		if (player < 0 || player >= MAX_PLAYERS) return;
		if (idx < 0 || idx >= DEADZONE_STEPS) idx = DEADZONE_DEFAULT_IDX;
		deadzoneIdx[player] = idx;
	}

	/* JOY_AXIS1_RIGHT..JOY_AXIS2_DOWN -> slot 0..7. Cualquier otro id -> -1.
	 * Sirve para distinguir una opcion de menu analogica de una normal sin
	 * mirar el tipoKey, que se sobrescribe al capturar la pulsacion. */
	static int analogSlot(int joyAxisId){
		if (joyAxisId >= JOY_AXIS1_RIGHT && joyAxisId <= JOY_AXIS2_DOWN){
			return joyAxisId - JOY_AXIS1_RIGHT;
		}
		return -1;
	}

	/* Slot al que pertenece una direccion fisica, o -1 si no es de un stick con
	 * nombre (por ejemplo el eje de los gatillos combinados de Windows).
	 * Miembro de la clase y no funcion libre: siendo 'static' en una cabecera que
	 * incluye medio proyecto, saldria un C4505 (funcion sin usar) en cada unidad
	 * que no la llame, que son casi todas. */
	static int analogSlotOfVirtual(int virtualIdx){
		int s;
		for (s = 0; s < ANALOG_TARGETS; s++){
			if (analogSlotAxis[s] == virtualIdx) return s;
		}
		return -1;
	}

	/* Sin la limpieza bidireccional de assignValue: dos direcciones distintas SI
	 * pueden convertirse legitimamente en lo mismo. */
	void setAnalogDst(int player, int slot, int encodedDst){
		if (player < 0 || player >= MAX_PLAYERS) return;
		if (slot < 0 || slot >= ANALOG_TARGETS) return;
		analogDst[player][slot] = encodedDst;
	}

	int getAnalogDst(int player, int slot){
		if (player < 0 || player >= MAX_PLAYERS) return -1;
		if (slot < 0 || slot >= ANALOG_TARGETS) return -1;
		return analogDst[player][slot];
	}

	template<size_t N, size_t M>
	void clear(int (&arr)[N][M], int value){
		for (int p=0; p < N; p++){
			for (int i=0; i < M; i++){
				arr[p][i] = value;
			}
		}
	}

	/* Aqui vivia isSameConfig(p1, p2). Estaba muerta -- la deduplicacion de perfiles la
	 * hace saveButtonsConfig por firma de texto -- y ademas comparaba solo hats, ejes y
	 * botones: al ignorar analogDst habria dado por iguales dos configuraciones que no
	 * lo son. Si algun dia hace falta, que se escriba comparando TODAS las tablas. */

	void setBtnFromSdl(int player, int sdlBtn, int btn){
		assignValue(sdlToBtn, btnToSdl, player, sdlBtn, btn);
	}

	void setHatFromSdl(int player, int sdlBtn, int btn){
		assignValue(sdlToHat, hatToSdl, player, sdlBtn, btn);
	}

	void setAxisFromSdl(int player, int sdlBtn, int btn){
		assignValue(sdlToAxis, axisToSdl, player, sdlBtn, btn);
	}

	/* 'btn' es un id de DESTINO en las tres. Devuelven el indice SDL asociado, o -1. */
	int getSdlHat(unsigned int player, unsigned int btn){
		if (player < MAX_PLAYERS && btn < MAX_TARGETS){
			return hatToSdl[player][btn];
		}
		return -1;
	}

	int getSdlBtn(unsigned int player, unsigned int btn){
		if (player < MAX_PLAYERS && btn < MAX_TARGETS){
			return btnToSdl[player][btn];
		}
		return -1;
	}

	int getSdlAxis(unsigned int player, unsigned int btn){
		if (player < MAX_PLAYERS && btn < MAX_TARGETS){
			return axisToSdl[player][btn];
		}
		return -1;
	}

	// Cambiamos M por M1 y M2 para permitir tamanyos distintos
	template<size_t N, size_t M1, size_t M2>
	void assignValue(int (&arrSdl)[N][M1], int (&arrBtn)[N][M2], int player, int sdlIdx, int coreIdx) {
		if (player < 0 || player >= (int)N) return;

		// 1. Limpiamos donde estuviera asignado el ID del core antes
		clearPrevious(arrSdl[player], coreIdx);
    
		// 2. Asignamos en la tabla SDL -> Core
		if (sdlIdx >= 0 && sdlIdx < (int)M1) {
			arrSdl[player][sdlIdx] = coreIdx;
		}

		// 3. Limpiamos donde estuviera asignado el ID de SDL antes
		clearPrevious(arrBtn[player], sdlIdx);
    
		// 4. Asignamos en la tabla Core -> SDL
		if (coreIdx >= 0 && coreIdx < (int)M2) {
			arrBtn[player][coreIdx] = sdlIdx;
		}
	}

    // Usamos una plantilla para detectar el tamanyo de la fila automaticamente
    template<size_t Size>
    void clearPrevious(int (&arr)[Size], int valueToClear) {
        // En VS2010, sizeof(arr) / sizeof(arr[0]) aqui Si funciona 
        // porque 'arr' es una referencia al array con su tamanyo real.
        for (size_t i = 0; i < Size; i++) {
            if (arr[i] == valueToClear) {
                arr[i] = -1;
            }
        }
    }
};

struct t_repeat_handler {
    Uint32 last_tick;
    bool repeat_mode;

    t_repeat_handler() : last_tick(0), repeat_mode(false) {}

    bool process(bool isPressed) {
        if (!isPressed) {
            last_tick = 0;
            repeat_mode = false;
            return false;
        }

        Uint32 now = SDL_GetTicks();
        if (last_tick == 0) { // Primera pulsacion
            last_tick = now;
            return true;
        }

        Uint32 elapsed = now - last_tick;
        Uint32 delay = repeat_mode ? 100 : 500; // 100ms rafaga, 500ms pausa inicial

        if (elapsed > delay) {
            last_tick = now;
            repeat_mode = true;
            return true;
        }
        return false;
    }
};

struct t_joy_state {
	//This two arrays are used mainly to know the state of the buttons while the core is running
	//They will be sent to the core
	bool btn_state[MAX_PLAYERS][MAX_SDL_BUTTONS];
	// Sticks analogicos como botones digitales
	bool axis_state[MAX_PLAYERS][MAX_SDL_AXIS_DIRS];
	// hats status
	bool hats_state[MAX_PLAYERS][MAX_SDL_HAT_VALUES];
	//To store the positions of the analog axis, but is not used by any core actually
	int16_t g_analog_state[MAX_PLAYERS][MAX_ANALOG_AXIS];

	/* Botones y hats SIMULADOS por una direccion de stick (ver analogDst). Van
	 * aparte y no sobre btn_state/hats_state para que no se pisen con el mando de
	 * verdad: si el stick y el boton fisico apuntan al mismo sitio, soltar el
	 * stick apagaria el boton aunque lo tuvieras pulsado. getCoreBtn/getCoreHat
	 * hacen el OR de los dos. */
	bool axisSimBtn[MAX_PLAYERS][MAX_SDL_BUTTONS];
	bool axisSimHat[MAX_PLAYERS][MAX_SDL_HAT_VALUES];

	uint16_t mouse_x;
	uint16_t mouse_y;
	int16_t mouse_rel_x;
	int16_t mouse_rel_y;
	bool mouse_buttons[3];
	
	// Keyboard state for retro_keyboard_event callback (overlay support)
	static const int MAX_RETRO_KEYS = 342;  // RETROK_LAST = 342
	t_key_input keyboard_state[MAX_RETRO_KEYS];
	/* La ULTIMA tecla soltada, no un array por tecla. Estaba declarada
	 * [MAX_RETRO_KEYS] pero se accedia siempre con '->', o sea el elemento 0: 341
	 * elementos muertos (5.456 B) y un tipo que aparentaba lo que no era. */
	t_key_input last_key_processed;

	/* Tecla pulsada desde el teclado en pantalla, con el instante en que hay que
	 * soltarla. Antes esto lo hacia un CreateThread POR PULSACION que escribia
	 * keyboard_state, dormia 50 ms y lo apagaba: carrera con el hilo principal, un
	 * hilo y un 'new' por tecla, y ademas despachaba el evento de teclado al core
	 * desde un hilo que no era el suyo. Ahora la suelta el poll normal. */
	struct t_sim_key {
		uint16_t retro_key;
		uint16_t retro_mod;
		uint32_t character;
		Uint32   releaseAt;
		bool     active;
		t_sim_key(): retro_key(0), retro_mod(0), character(0), releaseAt(0), active(false) {}
	};
	t_sim_key simKey;

	// Estados del frame anterior
    bool btn_last_state[MAX_PLAYERS][MAX_SDL_BUTTONS];
    bool axis_last_state[MAX_PLAYERS][MAX_SDL_AXIS_DIRS];
    bool hats_last_state[MAX_PLAYERS][MAX_SDL_HAT_VALUES];

	// Manejadores de repeticion
    t_repeat_handler btn_repeat[MAX_PLAYERS][MAX_SDL_BUTTONS];
    t_repeat_handler hat_repeat[MAX_PLAYERS][MAX_SDL_HAT_VALUES];
	t_repeat_handler axis_repeat[MAX_PLAYERS][MAX_SDL_AXIS_DIRS];

	t_joy_mapper mapperFrontend;
	t_joy_mapper mapperCore;
	t_joy_mapper mapperHotkeys;

	//Enables or disables the axis as pad only for the frontend
	bool frontAxisAsPad;

	/* 'names' es el nombre del PERFIL asignado a cada puerto: arranca siendo el del
	 * mando, pero loadButtonsRetro lo sobrescribe con el name= que venga del fichero.
	 *
	 * 'physicalNames' es el nombre que da SDL_JoystickName y NO lo pisa nadie. Hace
	 * falta separarlos para poder recuperar el perfil correcto segun el mando que
	 * este conectado: sin esto, tras la primera carga ya no habia forma de saber que
	 * mando hay de verdad en el puerto. Vacio = puerto sin mando. */
	std::string names[MAX_PLAYERS];
	std::string physicalNames[MAX_PLAYERS];
	int joyTypeIdx[MAX_PLAYERS];

	// Disparo rapido (turbo/autofire). rapidFire indexado por (jugador, id RETRO 0..15);
	// se usan solo los slots 0..15. Por defecto desactivado.
	bool rapidFire[MAX_PLAYERS][MAX_TARGETS];
	int  rapidFireRateIdx;   // 0=lento, 1=medio, 2=rapido
	bool turboPhaseOn;       // fase on/off, recalculada 1 vez por frame en retro_input_poll

	t_joy_state(){
		clear(btn_state);
		clear(axis_state);
		clear(hats_state);
		clear(axisSimBtn);
		clear(axisSimHat);
		clear(g_analog_state, 0);
		memset(joyTypeIdx, 0, sizeof(joyTypeIdx));
		memset(rapidFire, 0, sizeof(rapidFire));
		rapidFireRateIdx = 1;
		frontAxisAsPad = false;
		turboPhaseOn = true;
		mouse_x = mouse_y = mouse_rel_x = mouse_rel_y = 0;
		memset(mouse_buttons, 0, sizeof(mouse_buttons));
		for (int i = 0; i < MAX_RETRO_KEYS; ++i) {
			keyboard_state[i] = t_key_input();
		}
	}

	void clearAll(){
		clear(btn_state);
		clear(axis_state);
		clear(hats_state);
		clear(axisSimBtn);
		clear(axisSimHat);
		clear(g_analog_state, 0);
	}
	
	// llamar a esto AL FINAL de cada frame del bucle principal
    void updateLastState() {
        memcpy(btn_last_state, btn_state, sizeof(btn_state));
        memcpy(axis_last_state, axis_state, sizeof(axis_state));
        memcpy(hats_last_state, hats_state, sizeof(hats_state));
    }
	
	/* OR con axisSim*: una direccion de stick convertida en boton/cruceta es
	 * indistinguible aqui de una pulsacion real, que es lo que hace que funcione
	 * con cualquier core sin depender del tipo de dispositivo del puerto. */
	/* La cota superior de sdlBtn es nueva y hace falta: antes solo se comprobaba
	 * "> -1" porque el espacio de destinos y el espacio SDL median los dos 31, asi que
	 * el "btn < MAX_BUTTONS" de la izquierda acotaba los dos por accidente. Ahora que
	 * cada uno tiene su tamano, un id de destino valido puede traer un indice SDL que
	 * se salga del array de estado. */
	bool getCoreBtn(unsigned int player, unsigned int btn){
		int sdlBtn = mapperCore.getSdlBtn(player, btn);
		if (player < MAX_PLAYERS && sdlBtn > -1 && sdlBtn < MAX_SDL_BUTTONS){
			return btn_state[player][sdlBtn] || axisSimBtn[player][sdlBtn];
		}
		return false;
	}

	bool getCoreHat(unsigned int player, unsigned int btn){
		int sdlBtn = mapperCore.getSdlHat(player, btn);
		if (player < MAX_PLAYERS && sdlBtn > -1 && sdlBtn < MAX_SDL_HAT_VALUES){
			return hats_state[player][sdlBtn] || axisSimHat[player][sdlBtn];
		}
		return false;
	}

	bool getCoreAxis(unsigned int player, unsigned int btn){
		int sdlBtn = mapperCore.getSdlAxis(player, btn);
		if (player < MAX_PLAYERS && sdlBtn > -1 && sdlBtn < MAX_SDL_AXIS_DIRS){
			return axis_state[player][sdlBtn];
		}
		return false;
	}

	bool getCoreAny(unsigned int player, unsigned int btn){
		return getCoreBtn(player, btn) || getCoreHat(player, btn) || getCoreAxis(player, btn);
	}

	/* Magnitud 0..32767 de una direccion FISICA del stick (indice virtual
	 * eje*2+signo): solo la parte del recorrido que va en ese sentido. */
	int getPhysAxisMagnitude(unsigned int player, int virtualIdx){
		int axis, positive, raw;
		if (player >= MAX_PLAYERS || virtualIdx < 0) return 0;

		axis     = virtualIdx >> 1;
		positive = virtualIdx & 1;
		if (axis >= MAX_ANALOG_AXIS) return 0;

		raw = g_analog_state[player][axis];
		if (positive) return raw > 0 ? raw : 0;
		return raw < 0 ? -raw : 0;
	}

	/* Cuanto llega a una direccion analogica del core. Como el mapeo va de fisico
	 * a destino, hay que recorrer los ocho slots buscando los que APUNTEN aqui.
	 * Sumar permite que dos direcciones fisicas alimenten la misma del core. */
	int getAnalogTowards(unsigned int player, int coreVirtual){
		int total = 0, s;
		if (coreVirtual < 0) return 0;
		for (s = 0; s < ANALOG_TARGETS; s++){
			if (mapperCore.getAnalogDst(player, s) != coreVirtual) continue;
			total += getPhysAxisMagnitude(player, analogSlotAxis[s]);
		}
		return total;
	}

	/* Valor con signo de un eje del core, compuesto de sus dos direcciones.
	 * Restar una de otra da gratis el eje invertido y el cruzado entre sticks:
	 * basta con que los slots apunten a la direccion contraria o a la del otro. */
	int16_t getCoreAnalog(unsigned int player, int coreVirtualNeg, int coreVirtualPos){
		int v = getAnalogTowards(player, coreVirtualPos)
		      - getAnalogTowards(player, coreVirtualNeg);
		if (v >  32767) v =  32767;
		if (v < -32767) v = -32767;
		return (int16_t)v;
	}

	/* getBtn/getHat reciben un id de DESTINO; getSdlBtn/getSdlHat, un indice SDL crudo.
	 * Son dos espacios distintos, de ahi las dos constantes distintas. */
	bool getBtn(unsigned int player, unsigned int btn){
		int sdlBtn = mapperFrontend.getSdlBtn(player, btn);
		if (player < MAX_PLAYERS && sdlBtn > -1 && sdlBtn < MAX_SDL_BUTTONS){
			return btn_state[player][sdlBtn];
		}
		return false;
	}

	bool getSdlBtn(unsigned int player, unsigned int btn){
		if (player < MAX_PLAYERS && btn < MAX_SDL_BUTTONS){
			return btn_state[player][btn];
		}
		return false;
	}

	bool getHat(unsigned int player, unsigned int btn){
		int sdlBtn = mapperFrontend.getSdlHat(player, btn);
		if (player < MAX_PLAYERS && sdlBtn > -1 && sdlBtn < MAX_SDL_HAT_VALUES){
			return hats_state[player][sdlBtn];
		}
		return false;
	}

	bool getSdlHat(unsigned int player, unsigned int btn){
		if (player < MAX_PLAYERS && btn < MAX_SDL_HAT_VALUES){
			return hats_state[player][btn];
		}
		return false;
	}

	bool getBtnTap(unsigned int p, unsigned int b) { 
		int sdlIndex = mapperFrontend.getSdlBtn(p, b);
		return getTap(btn_state, btn_repeat, p, sdlIndex); 
	}
    
	bool getHatTap(unsigned int p, unsigned int h) { 
		int sdlIndex = mapperFrontend.getSdlHat(p, h);
		return getTap(hats_state, hat_repeat, p, sdlIndex); 
	}

	bool getAxisTap(unsigned int p, unsigned int h) { 
		int sdlIndex = mapperFrontend.getSdlAxis(p, h);
		return getTap(axis_state, axis_repeat, p, sdlIndex); 
	}

	bool getAnyTap(unsigned int p, unsigned int b) { 
		return getBtnTap(p,b) || getHatTap(p,b) || getAxisTap(p,b);
	}	

	// Helpers para tu mapper de Frontend
	bool getBtnReleased(unsigned int p, unsigned int b) { 
		int sdlIndex = mapperFrontend.getSdlBtn(p, b);
		return getReleased(btn_state, btn_last_state, p, sdlIndex); 
	}

	bool getHatReleased(unsigned int p, unsigned int h) { 
		int sdlIndex = mapperFrontend.getSdlHat(p, h);
		return getReleased(hats_state, hats_last_state, p, sdlIndex); 
	}

	bool getAxisReleased(unsigned int p, unsigned int a) { 
        int sdlIndex = mapperFrontend.getSdlAxis(p, a);
        return getReleased(axis_state, axis_last_state, p, sdlIndex); 
    }

    bool getAnyReleased(unsigned int p, unsigned int i) {
        return getBtnReleased(p, i) || getHatReleased(p, i) || getAxisReleased(p, i);
    }

	// Metodo generico para detectar el "Tap" con auto-repeat
    template<size_t N, size_t M>
    bool getTap(bool (&stateArray)[N][M], t_repeat_handler (&repeatArray)[N][M], int player, int index) {
        if (player < 0 || player >= (int)N || index < 0 || index >= (int)M) 
            return false;
	    return repeatArray[player][index].process(stateArray[player][index]);
    }

	template<size_t N, size_t M>
	void clear(bool (&arr)[N][M]){
		for (int p=0; p < N; p++){
			for (int i=0; i < M; i++){
				arr[p][i] = false;
			}
		}
	}

	template<size_t N, size_t M>
	void clear(int16_t (&arr)[N][M], int16_t value){
		for (int p=0; p < N; p++){
			for (int i=0; i < M; i++){
				arr[p][i] = value;
			}
		}
	}

	// Metodo generico para detectar cuando se suelta
	template<size_t N, size_t M>
	bool getReleased(bool (&curState)[N][M], bool (&lastState)[N][M], int player, int index) {
		if (player < 0 || player >= (int)N || index < 0 || index >= (int)M) 
			return false;
    
		// Si antes era true y ahora es false, es que se acaba de soltar
		return (lastState[player][index] == true && curState[player][index] == false);
	}

	
};

struct t_region{
	int selX;
	int selY;
	int selW;
	int selH;
	t_region() : selX(0), selY(0), selW(0), selH(0) {}
};

struct tEvento{
	int key;
	int keyMod;
	int unicode;
	int mouse;
	int mouse_x;
	int mouse_y;
	int mouse_state;
	t_region region;
	bool isMousedblClick;
	bool resize;
	bool isKey;
	bool isMouse;
	bool isMouseMove;
	bool isRegionSelected;
	bool quit;
	int width;
	int height;
		
	tEvento() 
		: key(0), keyMod(0), unicode(0), mouse(0), 
			mouse_x(0), mouse_y(0), mouse_state(0),
			isMousedblClick(false), resize(false), isKey(false), 
			isMouse(false), isMouseMove(false), isRegionSelected(false), 
			quit(false), width(0), height(0) {}
};

class ConfigEmu{
    public:
    ConfigEmu(){
        options_before_rom = false;
        use_rom_file = false;
        use_extension = true;
        use_rom_directory = true;
		generalConfig = false;
		no_uncompress = false;
		menu_show_directories = false;
		menu_directory_recursive = false;
		integerScale = false;
		aspectRatio = 0;
		shaderName = "";
		shaderMode = 0;
		scaleMode = 0;
		scaleIntMode = 0;
		execIdx = 0;
		syncMode = 0;
		music_file_index = 0;
    }
    ~ConfigEmu(){
    }

	bool generalConfig;

	std::string cfgFilePath;
	std::string internalName;
    std::string name;
    std::string system;
    std::string description;
    //Location of emulator, i.e. c:\mame
    std::string directory;
    //Name of emulator executable, i.e. mame.exe
    std::string executable;
	//List of cores available for the system to emulate
	std::vector<std::string> cores;
	//Index of the selected core executable
	int execIdx;
    //Global options passed to emulator, i.e. -sound 1
    std::string global_options;
    std::string map_file;
    //Options go before ROM when launching: "yes" or "no".
    // i.e. yes: "emulator.exe -option1 -option2 rom"
    //       no: "emulator.exe rom -option1 -option2"
    bool options_before_rom;
    std::string assets;
    /* Musica de menu propia de este core, ruta relativa al directorio de la app.
     * Vacia = usar la general de configMain[cfg::musicFile]. */
    std::string music_file;
	//Indice de la musica de fondo seleccionada
	int music_file_index;
    std::string screen_shot_directory;
    //# A ROM file is a list of ROMs to use.  If set to "no", ROMs are
    //# scanned for in the rom_directory.  If set to "yes" a ROM file (which
    //# is essentially just a list of ROMs) is used instead of trying scan.
    //# The default is "no".  ROM files are useful for merged ROMs with
    //# MAME, where the actual ROM names are buried within a ZIP file.
    bool use_rom_file;
    //Directory to ROMs
    std::string rom_directory;
    //List of possible ROM extensions (without the ".")
    std::string rom_extension;
    //Use extension when launching game: "yes" or "no"
    // i.e. yes: "emulator.exe rom.ext"
    //       no: "emulator.exe rom"
    bool use_extension;
    //Use rom_directory when launcher game: "yes" or "no"
    // i.e. yes: "emulator.exe c:\full\path\rom"
    //       no: "emulator.exe rom"
    bool use_rom_directory;
	//Avoids to uncompress the zip file
	bool no_uncompress;
	//Set the xml to obtain mame game names
	std::string mame_roms_xml;
	//Set the keyboard type
	std::string keyboard_type;
	//Show directories in the menu list
	bool menu_show_directories;
	//List files recursively
	bool menu_directory_recursive;
	//Set the default servers to connect when required
	std::string network_default_servers;
	//Se the default title
	std::string title_bkg_assets;
	//Override the main aspect ratio
	int aspectRatio;
	//Override the main shader mode. Se desdobla en dos:
	//  shaderName = lo que se PERSISTE en el .cfg (nombre del preset de
	//               assets\shaders; vacio = "Auto", usar el shader global).
	//  shaderMode = indice VIVO en la lista de shaders, +1 porque el 0 del
	//               menu es la entrada "Auto". Lo rellena
	//               CfgLoader::resolveShaderModes() al arrancar.
	std::string shaderName;
	int shaderMode;
	//Override the main scale mode
	int scaleMode;
	//Override the main Integer scale
	int integerScale;
	//Override the main Integer scale mode
	int scaleIntMode;
	//Override the main synchronization mode
	int syncMode;
};

struct t_rom_paths{
	std::string rompath;
	std::string savestate;
	std::string sram;
	std::string cht;        // ruta del fichero de cheats .cht del juego (formato RetroArch)
};

struct t_zipped_file_paths{
private:

public:
	// Route selected in the contents of the zip
	std::vector<std::string> pathInZip;
	// Directory where the file is
	std::string dir;
	// Name of the file. It usually is a zipped file
	std::string file;
	// Files listed on the zipped selected internal directory
	std::vector<ZipEntry> entries;
	//File extracted
	std::string extractedFile;

	void clear(){
		dir.clear();
		file.clear();
		pathInZip.clear();
		entries.clear();
		extractedFile.clear();
	}

	void cd(std::string newDir){
		pathInZip.push_back(newDir);
	}

	bool cdBack(){
		if (!pathInZip.empty()) {
			pathInZip.pop_back();
		}
		return !pathInZip.empty();
	}

	std::string getInternalDir(){
		std::string internalPath;
		for (unsigned int i=0; i < pathInZip.size(); i++){
			internalPath += pathInZip[i] + Constant::getFileSep();
		}
		return internalPath;
	}

	void setInternalDir(std::string dir){
		pathInZip = Constant::split(dir, Constant::getFileSep());
	}
};

struct t_dir_file_paths{
private:

public:
	// Route selected in the contents of the zip
	std::vector<std::string> relativePath;
	// Directory where the file is
	std::string dir;
	// Name of the selected file. It can be a directory or a regula file
	std::string file;

	void clear(){
		relativePath.clear();
		dir.clear();
		file.clear();
	}

	std::string getRelativePath(){
		std::string tmpPath = "";
		for (unsigned int i=0; i < relativePath.size(); i++){
			tmpPath += relativePath[i] + (i + 1 < relativePath.size() ? Constant::getFileSep() : "");
		}
		return tmpPath;
	}

	void addRelativePath(std::string dir){
		relativePath.push_back(dir);
	}

	void setRelativePath(std::string dir){
		relativePath = Constant::split(dir, Constant::getFileSep());
	}

	bool cdBack(){
		if (!relativePath.empty()) {
			relativePath.pop_back();
		}
		return !relativePath.empty();
	}
};