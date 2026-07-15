#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include <list>
#include <map>

#include <SDL.h>
#include <const/Constant.h>
#include <beans/structures.h>
#include <uiobjects/image.h>
#include <uiobjects/textarea.h>
#include <uiobjects/listmenu.h>
#include <menus/gestormenus.h>
#include <engine.h>
#include <http/scrapper.h>
#include <http/achievements.h>
#include <unzip/unziptool_common.h>
#include "unzip/ZipBrowser.h"
#include <io/video_common.h>
#include <io/dirutil.h>
#include <io/cfgloader.h>
#include <io/filepackage.h>
#include "menuassetloader.h"


#ifdef _XBOX
	#include <io/video_direct.h>
	extern "C" void XBOX_SetVideoFilter(int filterType);	
	extern "C" void XBOX_SelectEffect(int effectID);	
#else 
	#include <io/video.h>
	#include <io/hqx_2/hqx.h>
#endif


static const string SNAP = "snap";
static const string BOX2D = "box2d";
static const string SNAPTIT = "snaptit";
static const string SNAPFS = "snapFs";
static const string SYNOPSIS = "synopsis";
static const string YEAR = "year";
static const string MANUFACTURER = "manufacturer";
static const string SYSTEM = "system";
static const string MENUTMP = "menu.tmp";
static const string FS_IMAGES[] = {BOX2D, SNAP, SNAPTIT};

extern std::string videoScaleStrings[TOTAL_VIDEO_SCALE];
extern std::string aspectRatioStrings[TOTAL_VIDEO_RATIO];
extern std::string FRONTEND_BTN_TXT[MAXJOYBUTTONS];
extern t_rom_paths romPaths;

enum status_emu
{
	//The emulation has ben started and it's running
	EMU_STARTED = 0, 
	//The menu is showing so, the emulation is paused
	EMU_MENU, 
	EMU_MENU_OVERLAY,
	EMU_MENU_FILTER,
	EMU_MENU_IMAGE_VIEWER
};

enum FILE_STATUS
{
	FS_ZIP_NAVIGATION = 0, 
    FS_ZIP_FILE_EXTRACTED,
	FS_ZIP_EXTRACT_ERROR,
	FS_NOZIP_TO_LIST,
	FS_DIR_EMPTY,
	FS_DIR_NAVIGATION,
	FS_DIR_ISFILE
};

enum FILE_NAVIGATION
{
	FS_ZIP_CD = 0,
    FS_ZIP_CD_BACK,
	FS_DIR_CD,
	FS_DIR_BACK
};

struct t_achievement_surface{
	SDL_Surface *srf;
	SDL_Rect pos;
	SDL_Rect lastPos;

	t_achievement_surface(){
		pos.x = pos.y = pos.w = pos.h = 0;
		lastPos.x = lastPos.y = lastPos.w = lastPos.h = 0;
		srf = NULL;
	}
};

// Estructura auxiliar para ordenar por frecuencia
struct WordFreq {
    std::string word;
    int count;
    std::size_t originalOrder; // Para desempatar por orden de aparición

    bool operator<(const WordFreq& other) const {
        if (count != other.count) {
            return count > other.count; // Mayor frecuencia primero
        }
        return originalOrder < other.originalOrder; // Si empatan, el que apareció antes
    }
};

class GameMenu : public Engine{
    public:
        GameMenu(CfgLoader *cfgLoader);
        ~GameMenu();
		SDL_Surface *bg_screenshot;
		Image bg_image;
		Image title_image;
		GameTicks gameTicks;
		GestorMenus *configMenus;
		ScalerFunc current_scaler;
		int current_scaler_scale;
		void processConfigChanges();
		int *current_scaler_mode;
		int *current_ratio;
		int *current_shader;
		int *current_sync;
		bool *current_integer_scale;
		int *current_integer_scale_type;
		bool current_fast_forward;
		bool romLoaded;
		Uint32 uBkgColor;
		int selectedFsImage;

		void createMenuImages(ListMenu &);
        void loadEmuCfg(ListMenu &);
        void refreshScreen(ListMenu &);
		void processFrontendEvents(HOTKEYS_LIST);
		void processFrontendEventsAfter();
		void processHotkeys(HOTKEYS_LIST);
		bool emuCanLaunchGame();
        vector<string> launchProgram(const std::string& fullPathRom);
		FILE_STATUS listableZip(ListMenu &listMenu, FILE_NAVIGATION nav);
		FILE_STATUS listableDir(ListMenu &listMenu, FILE_NAVIGATION nav);
		string getSelectedRomFile(const ListMenu &listMenu);
		FILE_STATUS extractFileFromZip(const std::string& internalPath, const std::string& extractionPath, ZipBrowser& zb, ListMenu &listMenu);
		std::string GetMD5(const std::string& input);
        int saveGameMenuPos(ListMenu &);
        int recoverGameMenuPos(ListMenu &, struct ListStatus &);
        void showMessage(string);
		bool updateFps();
		CfgLoader * getCfgLoader();
        void setCfgLoader(CfgLoader *cfgLoader);
	    bool isDebug();
		FilePackage filePackage;
		
		void setEmuStatus(int tmpStat){
			if (status == EMU_MENU_IMAGE_VIEWER){
				//No queremos volver al visor de imagenes 
				lastStatus = EMU_MENU;
			} else {
				lastStatus = status;
			}
			status = tmpStat;
			//Siempre que cambiemos de estado de emulacion,
			//reseteamos los botones del joystick
			joystick->inputs.clearAll();

			if (status == EMU_STARTED && lastStatus != EMU_STARTED){
				BadgeDownloader::instance().stop();
				//Restauramos el shader porque parece haber algun problema con HLSLBackground::draw
				XBOX_SelectEffect(*current_shader);
			}
		}

		int getEmuStatus(){return status;}
		int getLastStatus(){return lastStatus;}
		bool isOnscreenKeybEnabled(){
			return onscreenKeyboard;
		}
		void setOnscreenKeyboard(bool enabled){
			onscreenKeyboard = enabled;
		}
		void setRomPaths(std::string rp);
		std::string getSramPath();
		void showSystemMessage(std::string, uint32_t);
		void showLangSystemMessage(std::string, uint32_t);
		void startScrapping();
		void loadGameAchievements(unzippedFileInfo& unzipped);
		void showAchievementMessage(const std::string &line1Str, const std::string &line2Str, const std::string &line3Str, SDL_Surface *badge);
		void clearOverlay();
		void clearOverlayRect(SDL_Rect&);
		void fillOverlay(int colorIndex);
		void fillOverlayAlpha(int colorIndex, int alpha);
		SDL_Surface* clonarPantalla(SDL_Surface*, int);
		bool loadBgImage();
		bool someImageLoaded();
		void nextImageLoaded();
		void prevImageLoaded();
		void findFirstImage();
		struct retro_system_av_info getAvInfo();
    private:
		std::vector<Message> messages;
		th_messages messagesAchievement;
		Scrapper scrapper;
		
		Image fsImage;

		map<int,int> gsTogdGameid;
		CfgLoader *cfgLoader;
		int status;
		int lastStatus;
		bool onscreenKeyboard;
		SDL_Rect rectFps;
		Uint32 bkgTextFps;
		SDL_Surface* fpsSurface;
		SDL_Surface* cpuSurface;
		SDL_Surface* memSurface;
		uint32_t lastFpsUpdate;
		uint32_t lastMemUpdate;
		bool *mustUpdateFps;
        std::map<std::string, Image> menuImages;
        std::map<std::string, TextArea> menuTextAreas;
		t_achievement_surface achievement_surface;
		SDL_Surface *filterAlphaRec;
		SDL_Surface *infoBtnSrf;
		int face_h_big;
		int face_h_small;

		// Carga asincrona del panel de assets — ver MenuAssetLoader arriba.
		friend class MenuAssetLoader;
		CRITICAL_SECTION m_csSnap;
		CRITICAL_SECTION m_csBox2d;
		CRITICAL_SECTION m_csSnaptit;
		CRITICAL_SECTION m_csSynopsis;
		bool             m_csInited;
		MenuAssetLoader  m_menuAssetLoader;

		bool cargarSystemAchievementTranslation(const std::string& nombreArchivo);
		int translateSystemAchievement();
		std::string configButtonsJOY();
		void processMessages();
		void processMessagesAchievements();
		void renderTrackers();
		void renderChallenges();
		void renderProgress();
		void selectScalerMode(int);
		void processKeyUp();
		void addControlerButtons(Menu*& menuControlesPuerto, int numPlayer);
		void showScrapProcess(ListMenu &listMenu);
		void initAchievements();
        std::string encloseWithCharIfSpaces(std::string, std::string);
		inline void updateAchievementsState(uint32_t currentTicks);
		inline void handleMessageQueue(uint32_t currentTicks);
		void renderCurrentAchievement();
		void clearLastAchievementArea();
		void drawSelectedKey(TTF_Font* font, t_keyboard& keyb, int row, int col);
		void drawKeyboard(TTF_Font* font, t_keyboard& keyb);
		void drawFilters(ListMenu &listMenu);
		void drawInfoButtons(SDL_Rect &rect);
		void drawTitle(ListMenu &listMenu, TTF_Font *fontBig);
		void drawSelectedGameAssets(ListMenu &listMenu, GameFile *game);
		string getAssetsDir(ConfigEmu *emu);
		std::string reduceWords(const std::string &sentence1, const std::string &sentence2);
};
