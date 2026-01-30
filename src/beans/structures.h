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

struct t_region{
    int selX;
    int selY;
    int selW;
    int selH;
};

struct tEvento{
    int key;
    int keyMod;
    int unicode;
    int joy;
	int sdljoybtn;
    int mouse;
    int mouse_x;
    int mouse_y;
    int mouse_state;
    t_region region;
    bool isMousedblClick;
    bool resize;
    bool isJoy;
    bool isKey;
    bool isMouse;
    bool isMouseMove;
    bool isRegionSelected;
    bool quit;
    bool keyjoydown;
    int width;
    int height;
	//Uint32 lastPressTime[MAXJOYBUTTONS];
	bool longKeyPress[MAXJOYBUTTONS];
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


class GameFile{
    public:
    GameFile(){
    }
    ~GameFile(){
    }
    std::string shortFileName;
    std::string longFileName;
    std::string gameTitle;
    std::size_t cutTitleIdx;
    //std::string gameImage;
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

#define MAX_BUTTONS 20
#define MAX_AXIS 20
#define MAX_HATS 10

struct t_joy_retro_inputs {
	//Cada posicion de los siguientes array corresponde a un boton del joystick
	int8_t buttons[MAX_BUTTONS];
    int8_t axis[MAX_AXIS];
    int8_t hats[MAX_HATS];
    
    std::string joyName;
    uint8_t nButtons;
    uint8_t nAxis;
    uint8_t nHats;
    bool axisAsPad;

	t_joy_retro_inputs(){
		nButtons = MAX_BUTTONS;
		nAxis = MAX_AXIS;
		nHats = MAX_HATS;
		axisAsPad = false;
		std::fill(buttons, buttons + MAX_BUTTONS, -1);
		std::fill(axis, axis + MAX_AXIS, -1);
		std::fill(hats, hats + MAX_HATS, -1);
	}

	template <size_t N>
	uint8_t findButtonIdx(int8_t btn, int8_t (&arr)[N]) {
		for (uint8_t i = 0; i < N; i++) {
			if (arr[i] == btn) return i;
		}
		return -1;
	}

	bool equals(const t_joy_retro_inputs& p) const {
        if (nButtons != p.nButtons || nAxis != p.nAxis || nHats != p.nHats || axisAsPad != p.axisAsPad) return false;
        if (nButtons > 0 && buttons && p.buttons)
            for (int i = 0; i < nButtons; i++) if (buttons[i] != p.buttons[i]) return false;
        if (nAxis > 0 && axis && p.axis)
            for (int i = 0; i < nAxis; i++) if (axis[i] != p.axis[i]) return false;
        if (nHats > 0 && hats && p.hats)
            for (int i = 0; i < nHats; i++) if (hats[i] != p.hats[i]) return false;
        return true;
    }

	/**
	* idx: Representa el numero de boton pulsado, ya sea obtenido mediante SDL, allegro...
	* btn: Representa el boton de libretro RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A, ...
	*/
	void setButton(uint8_t idx, int8_t btn){
		if (idx < nButtons){
			buttons[idx] = btn;
		}
	}

	/**
	* idx: Representa el numero de boton pulsado, ya sea obtenido mediante SDL, allegro...
	*/
	int8_t getButton(uint8_t idx){
		if (idx < nButtons){
			return buttons[idx];
		} else {
			return -1;
		}
	}

	/**
	* idx: Representa el numero de boton pulsado, ya sea obtenido mediante SDL, allegro...
	* btn: Representa el boton de libretro RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A, ...
	*/
	void setAxis(uint8_t idx, int8_t btn){
		if (idx < nAxis){
			axis[idx] = btn;
		}
	}

	/**
	* idx: Representa el numero de boton pulsado, ya sea obtenido mediante SDL, allegro...
	*/
	int8_t getAxis(uint8_t idx){
		if (idx < nAxis){
			return axis[idx];
		} else {
			return -1;
		}
	}

	/**
	* idx: Representa el numero de boton pulsado, ya sea obtenido mediante SDL, allegro...
	* btn: Representa el boton de libretro RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A, ...
	*/
	void setHat(uint8_t idx, int8_t btn){
		if (idx < nHats){
			hats[idx] = btn;
		}
	}

	/**
	* idx: Representa el numero de boton pulsado, ya sea obtenido mediante SDL, allegro...
	*/
	int8_t getHat(uint8_t idx){
		if (idx < nHats){
			return hats[idx];
		} else {
			return -1;
		}
	}
};

class ConfigEmu{
    public:
    ConfigEmu(){
        options_before_rom = false;
        use_rom_file = false;
        use_extension = true;
        use_rom_directory = true;
		generalConfig = false;
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
};

