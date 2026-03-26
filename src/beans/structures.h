#pragma once

#include <string>
#include <vector>
#include <stdint.h>

#include <const/constant.h>
#include <const/menuconst.h>

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
    uint16_t ticks;
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
	bool force_fs;
};

// Buffer para alojar el resultado de Scale2x (ej. 320x224 -> 640x448)
// Reservamos para el caso máximo (ej. 512x512 -> 1024x1024)
// 2048 * 1152 permite hasta Scale4x de una imagen de 512x256 o xBRZ alto
static uint16_t temp_buffer[2048 * 1152]; // Ocupa aprox 4.5 MB

struct GameData {
    std::string description;
    std::string year;
    std::string manufacturer;
    std::string cloneof;  // Si está vacío, es un "parent" (original)
    std::string romof;
    std::string driverStatus;
    // Útil para saber si es clon rápidamente
    bool isClone() const { return !cloneof.empty(); }
};

class GameFile{
    public:
    GameFile() : gameData(NULL), systemid(0){
    }
    ~GameFile(){
    }

	const GameData *gameData;
    std::string shortFileName;
    std::string longFileName;
    std::string gameTitle;
    std::size_t cutTitleIdx;
	std::string sortKey; // Nombre en minúsculas pre-calculado
	int systemid;
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

#define MAX_BUTTONS MAXJOYBUTTONS
#define MAX_AXIS MAXJOYBUTTONS
#define MAX_HATS MAXJOYBUTTONS
#define MAX_ANALOG_AXIS 16


struct t_joy_mapper{
	int sdlToHat[MAX_PLAYERS][MAX_HATS];
	int sdlToAxis[MAX_PLAYERS][MAX_AXIS];
	int sdlToBtn[MAX_PLAYERS][MAX_BUTTONS];

	int hatToSdl[MAX_PLAYERS][MAX_HATS];
	int axisToSdl[MAX_PLAYERS][MAX_AXIS];
	int btnToSdl[MAX_PLAYERS][MAX_BUTTONS];

	t_joy_mapper(){
		clear(sdlToHat, -1);
		clear(sdlToAxis, -1);
		clear(sdlToBtn, -1);

		clear(hatToSdl, -1);
		clear(axisToSdl, -1);
		clear(btnToSdl, -1);
	}

	template<size_t N, size_t M>
	void clear(int (&arr)[N][M], int value){
		for (int p=0; p < N; p++){
			for (int i=0; i < M; i++){
				arr[p][i] = value;
			}
		}
	}

	bool isSameConfig(int p1, int p2) {
		// Comparamos los arrays de hats, ejes y botones para ambos jugadores
		bool hats_iguales = memcmp(sdlToHat[p1], sdlToHat[p2], sizeof(int) * MAX_HATS) == 0;
		bool axis_iguales = memcmp(sdlToAxis[p1], sdlToAxis[p2], sizeof(int) * MAX_AXIS) == 0;
		bool btns_iguales = memcmp(sdlToBtn[p1], sdlToBtn[p2], sizeof(int) * MAX_BUTTONS) == 0;

		return hats_iguales && axis_iguales && btns_iguales;
	}

	void setBtnFromSdl(int player, int sdlBtn, int btn){
		assignValue(sdlToBtn, btnToSdl, player, sdlBtn, btn);
	}

	void setHatFromSdl(int player, int sdlBtn, int btn){
		assignValue(sdlToHat, hatToSdl, player, sdlBtn, btn);
	}

	void setAxisFromSdl(int player, int sdlBtn, int btn){
		assignValue(sdlToAxis, axisToSdl, player, sdlBtn, btn);
	}

	int getSdlHat(unsigned int player, unsigned int btn){
		if (player < MAX_PLAYERS && btn < MAX_HATS){
			return hatToSdl[player][btn];
		}
		return -1;
	}

	int getSdlBtn(unsigned int player, unsigned int btn){
		if (player < MAX_PLAYERS && btn < MAX_BUTTONS){
			return btnToSdl[player][btn];
		}
		return -1;
	}

	int getSdlAxis(unsigned int player, unsigned int btn){
		if (player < MAX_PLAYERS && btn < MAX_AXIS){
			return axisToSdl[player][btn];
		}
		return -1;
	}

	// Cambiamos M por M1 y M2 para permitir tamaños distintos
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

    // Usamos una plantilla para detectar el tamaño de la fila automáticamente
    template<size_t Size>
    void clearPrevious(int (&arr)[Size], int valueToClear) {
        // En VS2010, sizeof(arr) / sizeof(arr[0]) aquí SÍ funciona 
        // porque 'arr' es una referencia al array con su tamaño real.
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
        if (last_tick == 0) { // Primera pulsación
            last_tick = now;
            return true;
        }

        Uint32 elapsed = now - last_tick;
        Uint32 delay = repeat_mode ? 100 : 500; // 100ms ráfaga, 500ms pausa inicial

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
	bool btn_state[MAX_PLAYERS][MAX_BUTTONS];
	// Sticks analógicos como botones digitales
	bool axis_state[MAX_PLAYERS][MAX_AXIS];    
	// hats status
	bool hats_state[MAX_PLAYERS][MAX_HATS];    
	//To store the positions of the analog axis, but is not used by any core actually
	int16_t g_analog_state[MAX_PLAYERS][MAX_ANALOG_AXIS];

	// NUEVO: Estados del frame anterior
    bool btn_last_state[MAX_PLAYERS][MAX_BUTTONS];
    bool axis_last_state[MAX_PLAYERS][MAX_AXIS];
    bool hats_last_state[MAX_PLAYERS][MAX_HATS];

	// Manejadores de repetición
    t_repeat_handler btn_repeat[MAX_PLAYERS][MAX_BUTTONS];
    t_repeat_handler hat_repeat[MAX_PLAYERS][MAX_HATS];
	t_repeat_handler axis_repeat[MAX_PLAYERS][MAX_AXIS];

	t_joy_mapper mapperFrontend;
	t_joy_mapper mapperCore;
	t_joy_mapper mapperHotkeys;

	std::string names[MAX_PLAYERS];
	bool axisAsPad[MAX_PLAYERS];

	t_joy_state(){
		clear(btn_state);
		clear(axis_state);
		clear(hats_state);
		clear(g_analog_state, 0);
	}

	void clearAll(){
		clear(btn_state);
		clear(axis_state);
		clear(hats_state);
		clear(g_analog_state, 0);
	}
	
	// llamar a esto AL FINAL de cada frame del bucle principal
    void updateLastState() {
        memcpy(btn_last_state, btn_state, sizeof(btn_state));
        memcpy(axis_last_state, axis_state, sizeof(axis_state));
        memcpy(hats_last_state, hats_state, sizeof(hats_state));
    }
	
	bool getCoreBtn(unsigned int player, unsigned int btn){
		int sdlBtn = mapperCore.getSdlBtn(player, btn);
		if (player < MAX_PLAYERS && btn < MAX_BUTTONS && sdlBtn > -1){
			return btn_state[player][sdlBtn];
		}
		return false;
	}

	bool getCoreHat(unsigned int player, unsigned int btn){
		int sdlBtn = mapperCore.getSdlHat(player, btn);
		if (player < MAX_PLAYERS && btn < MAX_HATS && sdlBtn > -1){
			return hats_state[player][sdlBtn];
		}
		return false;
	}

	bool getCoreAxis(unsigned int player, unsigned int btn){
		int sdlBtn = mapperCore.getSdlAxis(player, btn);
		if (player < MAX_PLAYERS && btn < MAX_AXIS && sdlBtn > -1){
			return axis_state[player][sdlBtn];
		}
		return false;
	}

	bool getCoreAny(unsigned int player, unsigned int btn){
		return getCoreBtn(player, btn) || getCoreHat(player, btn) || getCoreAxis(player, btn);
	}

	bool getBtn(unsigned int player, unsigned int btn){
		int sdlBtn = mapperFrontend.getSdlBtn(player, btn);
		if (player < MAX_PLAYERS && btn < MAX_BUTTONS  && sdlBtn > -1){
			return btn_state[player][sdlBtn];
		}
		return false;
	}

	bool getSdlBtn(unsigned int player, unsigned int btn){
		if (player < MAX_PLAYERS && btn < MAX_BUTTONS){
			return btn_state[player][btn];
		}
		return false;
	}

	bool getHat(unsigned int player, unsigned int btn){
		int sdlBtn = mapperFrontend.getSdlHat(player, btn);
		if (player < MAX_PLAYERS && btn < MAX_HATS && sdlBtn > -1){
			return hats_state[player][sdlBtn];
		}
		return false;
	}

	bool getSdlHat(unsigned int player, unsigned int btn){
		if (player < MAX_PLAYERS && btn < MAX_HATS){
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

	// Método genérico para detectar el "Tap" con auto-repeat
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

	// Método genérico para detectar cuando se suelta
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
    }
    ~ConfigEmu(){

    }

	bool generalConfig;

	std::string internalName;
    std::string name;
    std::string system;
    std::string description;
    //Location of emulator, i.e. c:\mame
    std::string directory;
    //Name of emulator executable, i.e. mame.exe
    std::string executable;
	std::vector<std::string> cores;
    //Global options passed to emulator, i.e. -sound 1
    std::string global_options;
    std::string map_file;
    //Options go before ROM when launching: "yes" or "no".
    // i.e. yes: "emulator.exe -option1 -option2 rom"
    //       no: "emulator.exe rom -option1 -option2"
    bool options_before_rom;

    std::string assets;
    
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
};

struct t_rom_paths{
	std::string rompath;
	std::string savestate;
	std::string sram;
};