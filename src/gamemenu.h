#pragma once

#include <SDL.h>
#include <const/Constant.h>
//#include "io/screen.h"
//#include "io/sound.h"
//#include "utils/so/launcher.h"


#include <uiobjects/image.h>
#include <uiobjects/textarea.h>
#include <uiobjects/listmenu.h>
#include <menus/gestormenus.h>
#include <io/cfgloader.h>
#include <engine.h>

#include <memory>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <io/video.h>

static const string SNAP = "snap";
static const string BOX2D = "box2d";
static const string SNAPTIT = "snaptit";
static const string SNAPFS = "snapFs";
static const string SYNOPSIS = "synopsis";
static const string MENUTMP = "menu.tmp";

enum status_emu
{
	//The emulation has ben started and it's running
	EMU_STARTED = 0, 
	//The menu is showing so, the emulation is paused
	EMU_MENU
};

struct Message {
    std::string content;
    Uint32 ticks;
    Uint32 timeout;
    SDL_Surface* cache; // Nueva superficie para el mensaje renderizado
    SDL_Rect rect;      // Para guardar el tamaño y posición calculados

	Message(){
		cache = NULL;
		ticks = 0;
		timeout = 0;
		content = "";
	}
};


class GameMenu : public Engine{
    public:
        GameMenu(CfgLoader *cfgLoader);
        ~GameMenu();
        
		SDL_Surface *video_page;
		GameTicks gameTicks;
		GestorMenus *configMenus;

		Uint32 uBkgColor;

		void createMenuImages(ListMenu &);
        void loadEmuCfg(ListMenu &);
        void refreshScreen(ListMenu &);
		void processFrontendEvents();
		void processFrontendEventsAfter();
		void processMessages();
		void processHotkeys();

        vector<string> launchProgram(ListMenu &);
        bool initDblBuffer(int w, int h);
        int saveGameMenuPos(ListMenu &);
        int recoverGameMenuPos(ListMenu &, struct ListStatus &);
        void showMessage(string);
        ConfigEmu *getNextCfgEmu();
        ConfigEmu *getPrevCfgEmu();
		ConfigEmu *getCfgEmu();
		void updateFps();
		CfgLoader * getCfgLoader();
        void setCfgLoader(CfgLoader *cfgLoader);
	    bool isDebug();
		bool romLoaded;
		void setEmuStatus(int tmpStat){lastStatus = status;status = tmpStat;}
		int getEmuStatus(){return status;}
		int getLastStatus(){return lastStatus;}

		void showSystemMessage(std::string, uint32_t);
		
		// En la clase Config o GameMenu
		ScalerFunc current_scaler;
		int current_scaler_scale;
		void processConfigChanges();
		int *current_scaler_mode;
		int *current_ratio;
		int *current_sync;

    private:
		std::string configButtonsJOY();
		void selectScalerMode(int);

        int emuCfgPos;
		bool dblBufferEnabled;
		std::string getPathPrefix(std::string);
        std::string encloseWithCharIfSpaces(std::string, std::string);
        CfgLoader *cfgLoader;
        std::map<std::string, Image> menuImages;
        std::map<std::string, TextArea> menuTextAreas;
		void blit(SDL_Surface *, SDL_Surface *, int, int, int, int, int, int);
		int status;
		int lastStatus;
		SDL_Rect rectFps;
		Uint32 bkgTextFps;
		SDL_Surface* fpsSurface;
		uint32_t lastFpsUpdate;
		void addControlerButtons(Menu*& menuControlesPuerto, int numPlayer);
		Message message;
};
