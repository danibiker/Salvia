#include "constant.h"

std::string Constant::appDir;
std::string Constant::appExecutable;
char Constant::tempFileSep[2];
svColor Constant::colors[clTotalColors] = {
			{{0, 0, 0} , 0},			//clBackground
			{{0, 0, 0} , 0},			//clBlack
			{{0xFF, 0xFF, 0xFF} , 0},	//clWhite
			{{247, 221, 114}, 0},		//clBkgMenu
			{{255, 0, 0}, 0},			//clRed
			{{128, 128, 128}, 255},		//clMenuBars
			{{138, 207, 178}, 255},		//clTxtNavBar
}; 

const char *MEDIAS_TO_FIND[] = {"sstitle", "ss", "box-2D"};
const char *ASSETS_DIR[] = {"snaptit", "snap", "box2d", "synopsis"};
const std::string CFG_EXT = ".cfg";
const std::string RETROPAD_INI = "retropad.ini";
const std::string ROUTE_ACHIEVEMENT_TRANSLATIONS = "\\assets\\extra\\achievement_translations.cfg";
const std::string ROUTE_SCRAP_TRANSLATIONS = "\\assets\\extra\\scrap_translations.cfg";
const std::string PREFIX_DEFAULTS = "defaults_";
const std::string BG_FILENAME = "background";
//Url to obtain a list of available quake  servers
const std::string QUAKE_LIST_URL = "https://www.quakeservers.net/quake/servers/";
//Urls to download maps
const std::string QUAKE_MAPS_URL[QUAKE_MAPS_COUNT] = {
    "https://quakeone.com/qrack/maps/" 
	//,"https://maps.quakeworld.nu/all/"
};
const std::string START_FROM_EXCEPTION = "%$_START_FROMEXCEPTION_$%";
const std::string SCRAPPING_DAT = "images.bin";
const std::string PASS_MASK = "****";

const char *ICONS_PATH[] = {"menu_log.png",
	"folder.png",
	"file.png",
	"file.png",
	"zip.png",
	"image.png",
	"zip.png",
	"menu_osd.png",
	"setting.png",
	"core-options.png",
	"subsetting.png",
	"core-input-remapping-options.png",
	"loadstate.png",
	"menu_saving.png",
	"resume.png",
	"screenshot.png",
	"achievement-list.png",
	"menu_shutdown.png"
};

#ifdef _XBOX
	const char *SDL_BTN_TO_XBOX[12] = {"A", "B", "X", "Y", "L", "R", "L3", "R3", "Start", "Select", "L2", "R2"};
#else
	const char *SDL_BTN_TO_XBOX[12] = {"A", "B", "X", "Y", "L", "R", "Select", "Start", "L3", "R3", "", ""};
#endif
//Translated later on the first lines of GestorMenus::inicializar
std::string SDL_JOY_TO_XBOX[6] = {"Left", "Right", "Up", "Down", "R2", "L2"};
std::string SDL_HAT_TO_XBOX[9] = {"","Up","Right", "", "Down", "","","", "Left"};


const char *JOY_DESCRIPTIONS[] = {"JOY_BUTTON_A",
            "JOY_BUTTON_B",
            "JOY_BUTTON_X",
            "JOY_BUTTON_Y",
            "JOY_BUTTON_L",
            "JOY_BUTTON_R",
            "JOY_BUTTON_SELECT",
            "JOY_BUTTON_START",
            "JOY_BUTTON_L3",
            "JOY_BUTTON_R3",
            "JOY_BUTTON_UP",
            "JOY_BUTTON_UPLEFT",
            "JOY_BUTTON_LEFT",
            "JOY_BUTTON_DOWNLEFT",
            "JOY_BUTTON_DOWN",
            "JOY_BUTTON_DOWNRIGHT",
            "JOY_BUTTON_RIGHT",
            "JOY_BUTTON_UPRIGHT",
            "JOY_BUTTON_VOLUP",
            "JOY_BUTTON_VOLDOWN",
            "JOY_BUTTON_CLICK",
            "JOY_AXIS1_RIGHT",
            "JOY_AXIS1_LEFT",
            "JOY_AXIS1_UP",
            "JOY_AXIS1_DOWN",
            "JOY_AXIS2_RIGHT",
            "JOY_AXIS2_LEFT",
            "JOY_AXIS2_UP",
            "JOY_AXIS2_DOWN",
            "JOY_AXIS_L2",
            "JOY_AXIS_R2",
            "MAXJOYBUTTONS"};

const char *ICONS_CARTS_PATH[] = {"Nintendo - Game Boy Advance-content.png",
	"Nintendo - Game Boy-content.png",
	"Sega - Master System - Mark III-content.png",
	"Sega - Mega Drive - Genesis-content.png",
	"Nintendo - Super Nintendo Entertainment System-content.png",
	"Sega - 32X-content.png",
	"Sega - Game Gear-content.png",
	"Sega - Mega-CD - Sega CD-content.png",
	"Nintendo - Nintendo Entertainment System-content.png",
	"NEC - PC Engine - TurboGrafx 16 (E)-content.png",
	"Sony - PlayStation-content.png",
	"NEC - PC Engine CD - TurboGrafx-CD-content.png",
	"Arcade - STGSingle-content.png",
	"SNK - Neo Geo Pocket Color-content.png",
	"Sinclair - ZX Spectrum-content.png",
	"Microsoft - MSX-content.png",
	"DOS-content.png",
	"DOOM-content.png",
	"NEC - PC Engine SuperGrafx-content.png",
	"Quake.png",
	"default-content.png"
};

Constant::Constant(){
}

Constant::~Constant(){
}