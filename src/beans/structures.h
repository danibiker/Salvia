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
		bool sonidoMode;
	
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