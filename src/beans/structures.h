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

struct t_joy_retro_inputs {
    t_retro_input *buttons;
    t_retro_input *axis;
    t_retro_input *hats;
    t_retro_input *retroButtons;
    
    std::string joyName;
    int nButtons;
    int nAxis;
    int nHats;
    bool axisAsPad;

    // 1. Constructor básico
    t_joy_retro_inputs() : buttons(NULL), axis(NULL), hats(NULL), retroButtons(NULL) {
        nButtons = 0; nAxis = 0; nHats = 0; 
        axisAsPad = false;
    }

    // 2. Destructor: Liberar memoria para evitar leaks
    ~t_joy_retro_inputs() {
        liberar();
    }

    // 3. Constructor de Copia: Necesario para std::map y pasar por valor
    t_joy_retro_inputs(const t_joy_retro_inputs& other) : buttons(NULL), axis(NULL), hats(NULL), retroButtons(NULL) {
        copiarDesde(other);
    }

    // 4. Operador de Asignación: Crucial para "buttonsMapperLibretro[joyId] = ..."
    t_joy_retro_inputs& operator=(const t_joy_retro_inputs& other) {
        if (this != &other) {
            liberar();
            copiarDesde(other);
        }
        return *this;
    }

    // --- Funciones auxiliares de gestión ---
    void liberar() {
        if (buttons) delete[] buttons;
        if (axis) delete[] axis;
        if (hats) delete[] hats;
        if (retroButtons) delete[] retroButtons;
        buttons = axis = hats = retroButtons = NULL;
    }

    void copiarDesde(const t_joy_retro_inputs& other) {
        joyName = other.joyName;
        nButtons = other.nButtons;
        nAxis = other.nAxis;
        nHats = other.nHats;
        axisAsPad = other.axisAsPad;

        if (other.nButtons > 0 && other.buttons) {
            buttons = new t_retro_input[nButtons];
            memcpy(buttons, other.buttons, nButtons * sizeof(t_retro_input));
        }
        if (other.nAxis > 0 && other.axis) {
            // Asumiendo que axis usa nAxis * 2 como en tu cargador previo
            axis = new t_retro_input[nAxis * 2];
            memcpy(axis, other.axis, (nAxis * 2) * sizeof(t_retro_input));
        }
        if (other.nHats > 0 && other.hats) {
            hats = new t_retro_input[nHats];
            memcpy(hats, other.hats, nHats * sizeof(t_retro_input));
        }
        if (other.retroButtons) {
            // Ajusta el tamaño según tu constante de botones de Libretro (ej. 16 o 20)
            retroButtons = new t_retro_input[20]; 
            memcpy(retroButtons, other.retroButtons, 20 * sizeof(t_retro_input));
        }
    }

    // --- Tus funciones originales de lógica ---
    void setButton(int sdlbtnidx, int retrobtn) {
        if (buttons && sdlbtnidx < nButtons) buttons[sdlbtnidx].joy = retrobtn;
        if (retroButtons && retrobtn >= 0 && retrobtn < 20) retroButtons[retrobtn].joy = sdlbtnidx;
    }

    void setHat(int retroidx, int sdlHat) {
        if (hats && retroidx < nHats) hats[retroidx].joy = sdlHat;
    }

    void setAxis(int sdlbtnidx, int retrobtn) {
        if (axis && sdlbtnidx < nAxis * 2) axis[sdlbtnidx].joy = retrobtn;
    }

    bool equals(const t_joy_retro_inputs& p) const {
        if (nButtons != p.nButtons || nAxis != p.nAxis || nHats != p.nHats || axisAsPad != p.axisAsPad) return false;
        if (nButtons > 0 && buttons && p.buttons)
            for (int i = 0; i < nButtons; i++) if (buttons[i].joy != p.buttons[i].joy) return false;
        if (nAxis > 0 && axis && p.axis)
            for (int i = 0; i < nAxis * 2; i++) if (axis[i].joy != p.axis[i].joy) return false;
        if (nHats > 0 && hats && p.hats)
            for (int i = 0; i < nHats; i++) if (hats[i].joy != p.hats[i].joy) return false;
        return true;
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