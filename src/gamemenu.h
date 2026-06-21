#pragma once

#include <SDL.h>
#include <const/Constant.h>
//#include "io/screen.h"
//#include "io/sound.h"
//#include "utils/so/launcher.h"

#include <beans/structures.h>
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
#include <io/video_common.h>
#include "unzip/ZipBrowser.h"


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
static const string YEAR = "year";
static const string MANUFACTURER = "manufacturer";
static const string SYSTEM = "system";
static const string MENUTMP = "menu.tmp";

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
	EMU_MENU_FILTER
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

class GameMenu; // fwd

/* MenuAssetLoader: worker dedicado a cargar de forma asincrona el panel
 * derecho del menu (snap/box2d/snaptit + textos year/manufacturer/system
 * y synopsis).  Permite que la navegacion del menu (lista izquierda)
 * responda inmediatamente aunque el PNG/text de la rom seleccionada
 * tarde en cargar.
 *
 * Cancelacion: cada submit() incrementa un contador.  Entre cada paso
 * de carga el worker compara y, si llego una peticion mas reciente,
 * abandona la actual y reentra con la nueva. */
class MenuAssetLoader {
public:
    MenuAssetLoader();
    ~MenuAssetLoader();

    void start(GameMenu* owner);
    void stop();

    /* Encola una nueva peticion. El worker cancela la anterior si seguia en
     * curso.  El worker maneja el synopsis (file IO + word-wrap) y las 3
     * imagenes; usa una TTF_Font* INDEPENDIENTE (creada por
     * Fonts::createIndependentFont) para evitar el race con la fuente
     * compartida del main thread. */
    void submit(const std::string& fileNoExt,
                const std::string& assetsDir,
                SDL_PixelFormat* format,
                int overlayW,
                int synopsisMaxW);

private:
    static DWORD WINAPI WorkerProc(LPVOID self_ptr);
    void run();

    GameMenu*           m_owner;
    CRITICAL_SECTION    m_reqCS;
    HANDLE              m_event;
    HANDLE              m_thread;
    volatile LONG       m_seqSubmitted;   // se incrementa en cada submit
    volatile bool       m_stop;
    bool                m_started;
    bool                m_reqCSInited;
    TTF_Font*           m_workerFont;     // copia INDEPENDIENTE para TTF en el worker

    // Datos de la peticion pendiente (protegidos por m_reqCS)
    std::string         m_pendFileNoExt;
    std::string         m_pendAssetsDir;
    SDL_PixelFormat*    m_pendFormat;
    int                 m_pendOverlayW;
    int                 m_pendSynopsisMaxW;

    MenuAssetLoader(const MenuAssetLoader&);
    MenuAssetLoader& operator=(const MenuAssetLoader&);
};

class GameMenu : public Engine{
    public:
        GameMenu(CfgLoader *cfgLoader);
        ~GameMenu();
		SDL_Surface *bg_screenshot;
		Image bg_image;
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
		bool romLoaded;
		Uint32 uBkgColor;

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
		void showAchievementMessage(std::string line1Str, std::string line2Str, std::string line3Str, SDL_Surface *badge, SDL_Rect& lastMessagesArea);
		void clearOverlay();
		void clearOverlayRect(SDL_Rect&);
		void fillOverlay(int colorIndex);
		void fillOverlayAlpha(int colorIndex, int alpha);
		SDL_Surface* clonarPantalla(SDL_Surface*, int);
		bool loadBgImage();
    private:
		std::vector<Message> messages;
		th_messages messagesAchievement;
		Scrapper scrapper;

		map<int,int> gsTogdGameid;
		bool cargarSystemAchievementTranslation(const std::string& nombreArchivo);
		int translateSystemAchievement();

		std::string configButtonsJOY();
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
		SDL_Rect lastMessagesArea;
		SDL_Surface *filterAlphaRec;

		// Carga asincrona del panel de assets — ver MenuAssetLoader arriba.
		friend class MenuAssetLoader;
		CRITICAL_SECTION m_menuAssetCS;   // protege accesos a menuImages/menuTextAreas en zonas concurrentes
		bool             m_menuAssetCSInited;
		MenuAssetLoader  m_menuAssetLoader;

		
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
};
