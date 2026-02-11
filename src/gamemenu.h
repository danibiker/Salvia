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
#include <io/dirutil.h>
#include <http/scrapper.h>

#ifdef _XBOX
	#include <io/video_direct.h>
	extern "C" void XBOX_SetVideoFilter(int filterType);	
	extern "C" void XBOX_SelectEffect(int effectID);	
#else 
	#include <io/video.h>
	#include <io/hqx_2/hqx.h>
#endif

#include <memory>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>


static const string SNAP = "snap";
static const string BOX2D = "box2d";
static const string SNAPTIT = "snaptit";
static const string SNAPFS = "snapFs";
static const string SYNOPSIS = "synopsis";
static const string MENUTMP = "menu.tmp";

int initHqxFilter();

enum status_emu
{
	//The emulation has ben started and it's running
	EMU_STARTED = 0, 
	//The menu is showing so, the emulation is paused
	EMU_MENU, 
	EMU_MENU_OVERLAY
};

class GameMenu : public Engine{
    public:
        GameMenu(CfgLoader *cfgLoader);
        ~GameMenu();
        
		SDL_Surface *video_page;
		SDL_Surface *bg_screenshot;
		GameTicks gameTicks;
		GestorMenus *configMenus;

		Uint32 uBkgColor;

		void createMenuImages(ListMenu &);
        void loadEmuCfg(ListMenu &);
        void refreshScreen(ListMenu &);
		void processFrontendEvents(HOTKEYS_LIST);
		void processFrontendEventsAfter();
		void processMessages();
		void processHotkeys(HOTKEYS_LIST);

        vector<string> launchProgram(ListMenu &);
        bool initDblBuffer(int w, int h);
        int saveGameMenuPos(ListMenu &);
        int recoverGameMenuPos(ListMenu &, struct ListStatus &);
        void showMessage(string);
        
		
		void updateFps();
		CfgLoader * getCfgLoader();
        void setCfgLoader(CfgLoader *cfgLoader);
	    bool isDebug();
		
		void setEmuStatus(int tmpStat){
			lastStatus = status;
			status = tmpStat;
			//Siempre que cambiemos de estado de emulacion,
			//reseteamos los botones del joystick
			joystick->inputs.clearAll();
		}

		int getEmuStatus(){return status;}
		int getLastStatus(){return lastStatus;}
		
		
		struct t_rom_paths{
			std::string rompath;
			std::string savestate;
			std::string sram;
			std::string saves;
		};

		t_rom_paths* getRomPaths(){return &romPaths;}
		void setRomPaths(std::string rp);
		void setSavePath();

		void showSystemMessage(std::string, uint32_t);
		
		// En la clase Config o GameMenu
		ScalerFunc current_scaler;
		int current_scaler_scale;
		void processConfigChanges();
		int *current_scaler_mode;
		int *current_ratio;
		int *current_sync;
		bool *current_force_fs;
		bool romLoaded;
		void startScrapping();
		

    private:
		std::string configButtonsJOY();
		SDL_Surface* clonarPantalla(SDL_Surface*, int);
		void selectScalerMode(int);
		void processKeyUp();
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
		SDL_Surface* cpuSurface;
		uint32_t lastFpsUpdate;
		void addControlerButtons(Menu*& menuControlesPuerto, int numPlayer);
		Message message;
		t_rom_paths romPaths;
		bool *mustUpdateFps;
		Scrapper *scrapper;
};
