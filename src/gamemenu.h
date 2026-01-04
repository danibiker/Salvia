#pragma once

#include <SDL.h>
#include <const/Constant.h>
//#include "io/screen.h"
//#include "io/sound.h"
//#include "utils/so/launcher.h"


#include <uiobjects/image.h>
#include <uiobjects/textarea.h>
#include <uiobjects/listmenu.h>
#include <io/cfgloader.h>
#include <engine.h>

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


class GameMenu : public Engine{
    public:
        GameMenu(CfgLoader *cfgLoader);
        ~GameMenu();
        
		SDL_Surface *video_page;
		GameTicks gameTicks;
		
		void createMenuImages(ListMenu &);
        void loadEmuCfg(ListMenu &);
        void refreshScreen(ListMenu &);
        void launchProgram(ListMenu &);
        bool initDblBuffer(int w, int h);
        int saveGameMenuPos(ListMenu &);
        int recoverGameMenuPos(ListMenu &, struct ListStatus &);
        void showMessage(string);
        ConfigEmu getNextCfgEmu();
        ConfigEmu getPrevCfgEmu();
        void setCfgLoader(CfgLoader *cfgLoader);
	    bool isDebug();

    private:
        int emuCfgPos;
		bool dblBufferEnabled;
		std::string getPathPrefix(std::string);
        std::string encloseWithCharIfSpaces(std::string, std::string);
        CfgLoader *cfgLoader;
        std::map<std::string, Image> menuImages;
        std::map<std::string, TextArea> menuTextAreas;
		void blit(SDL_Surface *, SDL_Surface *, int, int, int, int, int, int);
};
