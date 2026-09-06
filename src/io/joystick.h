#pragma once

#include <map>
#include <vector>

#include <libretro/libretro.h>
#include <io/cursorgestor.h>
#include <io/hotkeys.h>
#include <beans/structures.h>

//El comportamiento de un hat esta estandarizado por el propio API: todos los hats 
//se tratan como interruptores de posicion de 8 direcciones (mas la posicion centrada), 
//independientemente de como sea fisicamente el dispositivo.
#define MAX_HAT_POSITIONS 9

/* Acciones del frontend, en el orden en que salen en el menu de asignacion. La
 * POSICION es lo que manda: es el sufijo de la clave i18n (menu.controls.frontkeyN)
 * y el indice de FRONTEND_BTN_TXT. Anadir SIEMPRE al final.
 *
 * Los dos ultimos son los gatillos, que no son un boton en las dos plataformas: en
 * la 360 son botones SDL (10 y 11) y en Windows las dos mitades del eje 2. Por eso
 * se leen con getAnyTap, que mira las tres tablas del mapper. */
static int FRONTEND_BTN_VAL[] = {JOY_BUTTON_UP, JOY_BUTTON_DOWN, JOY_BUTTON_LEFT, JOY_BUTTON_RIGHT, JOY_BUTTON_A, JOY_BUTTON_B, JOY_BUTTON_Y,
	JOY_BUTTON_L, JOY_BUTTON_R, JOY_BUTTON_L3, JOY_AXIS_L2, JOY_AXIS_R2};

/* Botones del mando que salen en el menu, EN ORDEN LOGICO (el mismo del enum
 * joystickButtons). Su posicion i NO es el indice SDL: para eso esta
 * sdlIndexOfLogicalBtn, que es lo que usa configMapperRetro al poner los
 * defaults. */
static const int configurablePortButtons[] = {
	RETRO_DEVICE_ID_JOYPAD_A,
	RETRO_DEVICE_ID_JOYPAD_B,
	RETRO_DEVICE_ID_JOYPAD_X,
	RETRO_DEVICE_ID_JOYPAD_Y,
	RETRO_DEVICE_ID_JOYPAD_L,
	RETRO_DEVICE_ID_JOYPAD_R,
	RETRO_DEVICE_ID_JOYPAD_SELECT,
	RETRO_DEVICE_ID_JOYPAD_START,
	RETRO_DEVICE_ID_JOYPAD_L3,
	RETRO_DEVICE_ID_JOYPAD_R3,
	RETRO_DEVICE_ID_JOYPAD_L2,
	RETRO_DEVICE_ID_JOYPAD_R2
};

static const int configurablePortHats[] = {
	RETRO_DEVICE_ID_JOYPAD_UP,
	RETRO_DEVICE_ID_JOYPAD_DOWN,
	RETRO_DEVICE_ID_JOYPAD_LEFT,
	RETRO_DEVICE_ID_JOYPAD_RIGHT
};


static const int configurableSdlHats[] = {
	RETRO_DEVICE_ID_JOYPAD_UP,    // --> SDL_HAT_UP    = 0x01
	RETRO_DEVICE_ID_JOYPAD_RIGHT, // --> SDL_HAT_RIGHT = 0x02
	RETRO_DEVICE_ID_JOYPAD_DOWN,  // --> SDL_HAT_DOWN  = 0x04
	RETRO_DEVICE_ID_JOYPAD_LEFT  // --> SDL_HAT_LEFT  = 0x08
};

/* Indexado por direccion fisica (eje*2 + signo). Las cuatro primeras eran el
 * "stick izquierdo como cruceta"; ahora eso se expresa desde la entrada del stick
 * (analogDst), asi que quedan sin asignar.
 *
 * Lo unico que sobrevive es el eje 2 de Windows, que son los gatillos combinados:
 * no pertenece a ningun stick con nombre y se sigue digitalizando por esta via.
 * En la 360 ese mismo eje 2 es la X del stick DERECHO y los gatillos son botones,
 * asi que alli no hay nada que mapear aqui -- dejarlo apuntando a R2/L2 era dato
 * muerto y ademas contradecia al slot 5 de analogSlotAxis. */
#ifdef _XBOX
static const int configurableSdlAxis[] = {
	-1, -1, -1, -1,                 /* 0..3: stick izquierdo         */
	-1, -1                          /* 4,5 : eje 2 = stick derecho X */
};
#else
static const int configurableSdlAxis[] = {
	-1,                             /* 0: stick izq X- */
	-1,                             /* 1: stick izq X+ */
	-1,                             /* 2: stick izq Y- */
	-1,                             /* 3: stick izq Y+ */
	RETRO_DEVICE_ID_JOYPAD_R2,      /* 4: eje de gatillos, lado negativo */
	RETRO_DEVICE_ID_JOYPAD_L2       /* 5: eje de gatillos, lado positivo */
};
#endif

/* Las ocho direcciones de stick que salen en el menu, en su orden. El valor es el
 * id del enum joystickButtons; el slot se saca con t_joy_mapper::analogSlot(), y
 * la direccion fisica a la que corresponde con analogSlotAxis (structures.h). */
static const int configurablePortAnalogs[ANALOG_TARGETS] = {
	JOY_AXIS1_UP,    JOY_AXIS1_DOWN,  JOY_AXIS1_LEFT,  JOY_AXIS1_RIGHT,
	JOY_AXIS2_UP,    JOY_AXIS2_DOWN,  JOY_AXIS2_LEFT,  JOY_AXIS2_RIGHT
};

/* Igual que configurablePortButtons: va en orden LOGICO y el indice SDL de cada uno
 * lo da sdlBtnOf. Los dos gatillos ocupan las posiciones 10 y 11, donde sdlBtnOf cae
 * a la identidad y da los botones SDL 10 (LT) y 11 (RT) de la 360. En Windows el
 * mando no llega a esos botones y configMapperFrontend los salta solo; alli los
 * gatillos entran por configurableSdlFrontAxis. */
static const int configurableFrontButtons[] = {
	JOY_BUTTON_A, JOY_BUTTON_B, JOY_BUTTON_X, JOY_BUTTON_Y, JOY_BUTTON_L, JOY_BUTTON_R, JOY_BUTTON_SELECT, JOY_BUTTON_START, JOY_BUTTON_L3, JOY_BUTTON_R3,
	JOY_AXIS_L2, JOY_AXIS_R2
};

static const int configurableSdlFrontHats[] = {
	JOY_BUTTON_UP,    // --> SDL_HAT_UP    = 0x01
	JOY_BUTTON_RIGHT, // --> SDL_HAT_RIGHT = 0x02
	JOY_BUTTON_DOWN,  // --> SDL_HAT_DOWN  = 0x04
	JOY_BUTTON_LEFT   // --> SDL_HAT_LEFT  = 0x08
};

/* Indexada por direccion fisica (eje*2 + signo). Las cuatro primeras son el stick
 * izquierdo navegando el menu (solo vivas con "Analog pad" encendido).
 *
 * Las dos ultimas van con #ifdef por el mismo motivo que su gemela del core,
 * configurableSdlAxis: el eje 2 solo son los gatillos en Windows. En la 360 es la X
 * del stick DERECHO y los gatillos son botones, asi que dejarlo apuntando a R2/L2
 * ademas de ser dato muerto haria que el stick derecho disparase las acciones de
 * menu de los gatillos. */
#ifdef _XBOX
static const int configurableSdlFrontAxis[] = {
	JOY_BUTTON_LEFT,
	JOY_BUTTON_RIGHT,
	JOY_BUTTON_UP,
	JOY_BUTTON_DOWN,
	-1,                /* 4,5: eje 2 = stick derecho X */
	-1
};
#else
static const int configurableSdlFrontAxis[] = {
	JOY_BUTTON_LEFT,   
	JOY_BUTTON_RIGHT,
	JOY_BUTTON_UP,
	JOY_BUTTON_DOWN, 
	JOY_AXIS_R2,       /* eje de gatillos, lado negativo */
	JOY_AXIS_L2        /* eje de gatillos, lado positivo */
};
#endif

extern t_rom_paths romPaths;

struct t_controller_port {
	int current_device_id;			// ID seleccionado actualmente (ej. RETRO_DEVICE_JOYPAD)
	std::string current_desc;       // Descripcion amigable (ej. "SuperScope")
	// Lista de opciones que el core nos dio para este puerto
	std::vector<std::pair<unsigned, std::string>> available_types; 
	t_controller_port(){
		current_device_id = -1;
	}	
};


enum BTN_SHAPE{
	BS_CIRCLE,
	BS_DOUBLE_CIRCLE,
	BS_IMAGE
};

struct t_info_btn{
	BTN_SHAPE shape;
	std::string text;
	std::string description;
	bool mergeNext;
};

class Joystick{
    public:
        Joystick();
        ~Joystick();

		bool pollKeys(int);
		/* Posicion de cada raton fisico. spanW/spanH = superficie contra la que
		 * SDL acota su raton (GameMenu::getMouseSurface). Llamar 1 vez por poll. */
		void updateMice(int spanW, int spanH);
		bool init_all_joysticks();
		void close_joysticks();
		int getNumJoysticks(){return mNumJoysticks;}
		
		std::string saveButtonsRetroDefault();
		std::string saveButtonsRetroGame();
		std::string saveButtonsRetroCore();
		std::string saveButtonsConfig(std::string, bool=true);
		bool loadButtonsRetro(std::string);
		void updateTypes();
		HOTKEYS_LIST findHotkey();
		void setInfoButtons();

		// Get the joytype index and make sure that the one we select is not RETRO_DEVICE_NONE if we
		// have other options. Choose always RETRO_DEVICE_JOYPAD, if there was no device set
		int getCkeckedJoyTypeIndex(int);

		//Array para poder detectar la pulsacion del start en xbox360
		int8_t startHoldFrames[MAX_PLAYERS];
		//Joysticks abiertos
		SDL_Joystick* g_joysticks[MAX_PLAYERS];
		//Type of the controller ports
		t_controller_port g_ports[MAX_PLAYERS];
		//Mapeo de botones para los joysticks
		t_joy_state inputs;
		//Devolvemos un objeto evento en el caso de peticion de salida de SDL
		tEvento evento;
		//Se guardan las hotkeys en una clase aparte
		Hotkeys* hotkeys;
		//Se guardan los botones para mostrar informacion al usuario
		std::vector<t_info_btn> infoButtons;
		bool infoButtonsDirty;
		
    private:

		void configMapperRetro(t_joy_mapper& mapper, int joyId);
		void configMapperFrontend(t_joy_mapper& mapper, int joyId);
		template <size_t N>
		void cargarValoresEnArray(int8_t (&arr)[N], std::string str, int maxValues) {
			std::vector<std::string> v = Constant::splitChar(str, ',');
			for (int i=0; i < v.size() && i < maxValues; i++){
				arr[i] = Constant::strToTipo<int8_t>(v[i]);
			}
		}

		int w,h;
		int mNumJoysticks;
		bool ignoreButtonRepeats;
		int actualCursor;
		CursorGestor *gestorCursor;
		void setCursor(int cursor);
		
};

