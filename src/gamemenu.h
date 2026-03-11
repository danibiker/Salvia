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
#include <http/achievements.h>
#include <unzip/unziptool_common.h>

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
#include <list>
#include <map>


static const string SNAP = "snap";
static const string BOX2D = "box2d";
static const string SNAPTIT = "snaptit";
static const string SNAPFS = "snapFs";
static const string SYNOPSIS = "synopsis";
static const string MENUTMP = "menu.tmp";

int initHqxFilter();

extern std::string videoScaleStrings[TOTAL_VIDEO_SCALE];
extern std::string aspectRatioStrings[TOTAL_VIDEO_RATIO];
extern std::string FRONTEND_BTN_TXT[MAXJOYBUTTONS];

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
		struct t_rom_paths{
			std::string rompath;
			std::string savestate;
			std::string sram;
		};
		
		SDL_Surface *video_page;
		SDL_Surface *bg_screenshot;
		GameTicks gameTicks;
		GestorMenus *configMenus;
		ScalerFunc current_scaler;
		int current_scaler_scale;
		void processConfigChanges();
		int *current_scaler_mode;
		int *current_ratio;
		int *current_sync;
		bool *current_force_fs;
		bool romLoaded;
		Uint32 uBkgColor;

		void createMenuImages(ListMenu &);
        void loadEmuCfg(ListMenu &);
        void refreshScreen(ListMenu &);
		void processFrontendEvents(HOTKEYS_LIST);
		void processFrontendEventsAfter();
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

			if (status == EMU_STARTED && lastStatus != EMU_STARTED){
				BadgeDownloader::instance().stop();
			}
		}

		int getEmuStatus(){return status;}
		int getLastStatus(){return lastStatus;}
		t_rom_paths* getRomPaths(){return &romPaths;}
		void setRomPaths(std::string rp);
		std::string getSramPath();
		void showSystemMessage(std::string, uint32_t);
		void showLangSystemMessage(std::string, uint32_t);
		void startScrapping();
		void loadGameAchievements(unzippedFileInfo& unzipped);
		void showAchievementMessage(std::string line1Str, std::string line2Str, std::string line3Str, SDL_Surface *badge, SDL_Rect& lastMessagesArea);

    private:
		std::vector<Message> messages;
		std::list<AchievementState> messagesAchievement;
		map<int,int> gsTogdGameid;
		bool cargarSystemAchievementTranslation(const std::string& nombreArchivo);
		int translateSystemAchievement();

		std::string configButtonsJOY();
		CfgLoader *cfgLoader;
		int status;
		int lastStatus;
		SDL_Rect rectFps;
		Uint32 bkgTextFps;
		SDL_Surface* fpsSurface;
		SDL_Surface* cpuSurface;
		uint32_t lastFpsUpdate;
		t_rom_paths romPaths;
		bool *mustUpdateFps;
        std::map<std::string, Image> menuImages;
        std::map<std::string, TextArea> menuTextAreas;
		SDL_Rect lastMessagesArea;

		SDL_Surface* clonarPantalla(SDL_Surface*, int);
		void processMessages();
		void processMessagesAchievements();
		void renderTrackers();
		void renderChallenges();
		void renderProgress();
		void selectScalerMode(int);
		void processKeyUp();
		bool dblBufferEnabled;
		void blit(SDL_Surface *, SDL_Surface *, int, int, int, int, int, int);
		void addControlerButtons(Menu*& menuControlesPuerto, int numPlayer);
		void showScrapProcess(ListMenu &listMenu);
		void initAchievements();
		std::string getPathPrefix(std::string);
        std::string encloseWithCharIfSpaces(std::string, std::string);
		void updateAchievementsState(uint32_t currentTicks);
		void handleMessageQueue(uint32_t currentTicks);
		void renderCurrentAchievement();
		void clearLastAchievementArea();
};
