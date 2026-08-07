#include "constant.h"

std::string Constant::appDir;
std::string Constant::appExecutable;
char Constant::tempFileSep[2];
float aspectRatioValues [] = {4/3.0f, 4/3.0f, 3/2.0f, 8/7.0f, 10/9.0f, 1, 5/4.0f, 16/9.0f, 16/10.0f, -1};

svColor Constant::colors[clTotalColors] = {
	{{0, 0, 0} , 0},			//clBackground
	{{1, 1, 1} , 0},			//clBlack (1,1,1 en vez de 0,0,0 para que alpha-fixup shader lo distinga de píxel vacío)
	{{0xFF, 0xFF, 0xFF} , 0},	//clWhite
	{{241, 222, 19}, 255},		//clYellow
	{{255, 0, 0}, 0},			//clRed
	{{76, 194, 255}, 255},		//clBlue
	{{247, 221, 114}, 0},		//clBkgMenu
	{{237, 221, 150}, 255},		//clBkgMenuLighter
	{{40, 40, 40}, 0xFF},		//clBG
	{{200, 200, 200}, 255},		//clBorder
	{{128, 128, 128}, 255},		//clMenuBars
	{{138, 207, 178}, 255},		//clTxtNavBar
	{{160, 160, 160}, 255},		//clDarkGray
	{{200, 200, 200}, 255},		//clSwitchEnabled
	{{77, 77, 77}, 255},		//clSwitchEnabled
	{{57, 72, 93}, 255},		//clPaleBlue
	{{59,59,59}	 , 255},		//clAskTitle
	{{69,69,69}	 , 255},		//clAskBg 	
	{{91,91,91}	 , 255},		//clAskLine 
	{{190,190,190}, 255},		//clAskText 
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
const std::string TITLE_EMU_FILENAME = "title.png";
const std::string PASS_MASK = "****";

// Caracteres que queremos sustituir por un espacio (para no pegar palabras)
const char SYMBOLS_TO_SPACE[] = ":-._/\\|,;"; 

// Caracteres que queremos eliminar por completo (ruido)
const char SYMBOLS_TO_REMOVE[] = "\"\'!?*#¿¡";

const char *ICONS_PATH[] = {"menu_log.png",		// page_white_text
	"folder.png",								// folder
	"file.png",									// page_white
	"file.png",									// page_white_gear
	"zip.png",									// page_white_compressed
	"image.png",								// page_white_picture
	"zip.png",									// page_white_zip
	"menu_osd.png",								// ico_video
	"setting.png",								// ico_settings
	"core-options.png",							// ico_settings_core
	"subsetting.png",							// ico_subsettings
	"core-input-remapping-options.png",			// ico_remap
	"loadstate.png",							// ico_savestates
	"menu_saving.png",							// ico_saving
	"resume.png",								// ico_return
	"screenshot.png",							// ico_scrapper
	"achievement-list.png",						// ico_achievements
	"menu_shutdown.png",						// ico_shutdown
	"menu_help.png",							// ico_help
	"core-cheat-options.png"					// ico_cheats
};

const char *ICONS_CARTS_PATH[] = {"Nintendo - Game Boy Advance-content.png",  // cart_gba
	"Nintendo - Game Boy-content.png",										  // cart_gb
	"Sega - Master System-content.png",										  // cart_sms
	"Sega - Mega Drive - Genesis-content.png",								  // cart_genesis
	"Nintendo - SNES-content.png",											  // cart_snes
	"Sega - 32X-content.png",												  // cart_32x
	"Sega - Game Gear-content.png",											  // cart_gg
	"Sega - Mega-CD - Sega CD-content.png",									  // cart_mcd
	"Nintendo - NES-content.png",											  // cart_nes
	"NEC - PC Engine-content.png",											  // cart_pce
	"Sony - PlayStation-content.png",										  // cart_psx
	"NEC - PC Engine CD-content.png",										  // cart_pce_cd
	"Arcade - STGSingle-content.png",										  // cart_mame 
	"SNK - Neo Geo Pocket Color-content.png",								  // cart_neogeo_pocket
	"Sinclair - ZX Spectrum-content.png",									  // cart_zx
	"Microsoft - MSX-content.png",											  // cart_msx
	"DOS-content.png",														  // cart_dos
	"DOOM-content.png",														  // cart_doom
	"NEC - PC Engine SuperGrafx-content.png",								  // cart_supergrafx
	"Quake.png",															  // cart_quake
	"default-content.png",													  // cart_default
	"The 3DO Company - 3DO-content.png",									  // cart_3do
	"Bandai - WonderSwan-content.png",										  // cart_wonderswan
	"Nintendo - Virtual Boy-content.png",									  // cart_virtualboy
	"Atari - Lynx-content.png"												  // cart_atarilynx
};

// Nombres de sistema de libretro-database (ficheros rdb/ y carpetas cht/), indexado por
// el enum cart_*. Debe seguir EXACTAMENTE el orden del enum. "" = sistema sin base de
// cheats por CRC (arcade, DOS, Doom, Quake, default) -> no se intenta resolver/descargar.
const char *RDB_SYSTEM_NAMES[] = {
	"Nintendo - Game Boy Advance",                     // cart_gba
	"Nintendo - Game Boy",                             // cart_gb
	"Sega - Master System - Mark III",                 // cart_sms
	"Sega - Mega Drive - Genesis",                     // cart_genesis
	"Nintendo - Super Nintendo Entertainment System",  // cart_snes
	"Sega - 32X",                                      // cart_32x
	"Sega - Game Gear",                                // cart_gg
	"Sega - Mega-CD - Sega CD",                        // cart_mcd
	"Nintendo - Nintendo Entertainment System",        // cart_nes
	"NEC - PC Engine - TurboGrafx 16",                 // cart_pce
	"Sony - PlayStation",                              // cart_psx
	"NEC - PC Engine CD - TurboGrafx-CD",              // cart_pce_cd
	"",                                                // cart_mame (arcade: cheats por set, no por CRC)
	"SNK - Neo Geo Pocket Color",                      // cart_neogeo_pocket
	"Sinclair - ZX Spectrum",                          // cart_zx
	"Microsoft - MSX",                                 // cart_msx
	"",                                                // cart_dos
	"",                                                // cart_doom
	"NEC - PC Engine SuperGrafx",                      // cart_supergrafx
	"",                                                // cart_quake
	"",                                                // cart_default
	"The 3DO Company - 3DO",                           // cart_3do
	"Bandai - WonderSwan",                             // cart_wonderswan
	"Nintendo - Virtual Boy",                          // cart_virtualboy
	"Atari - Lynx"									   // cart_atarilynx
};

#ifdef _XBOX
	const char *SDL_BTN_TO_XBOX[12] = {"A", "B", "X", "Y", "LB", "RB", "L3", "R3", "Start", "Back", "LT", "RT"};
	std::string SDL_JOY_TO_XBOX[6] = {"Left", "Right", "Up", "Down", "LT", "RT"};
#else
	const char *SDL_BTN_TO_XBOX[12] = {"A", "B", "X", "Y", "L", "R", "Select", "Start", "L3", "R3", "", ""};
	std::string SDL_JOY_TO_XBOX[6] = {"Left", "Right", "Up", "Down", "R2", "L2"};
#endif
//Translated later on the first lines of GestorMenus::inicializar
std::string SDL_HAT_TO_XBOX[9] = {"","Up","Right", "", "Down", "","","", "Left"};
std::string FRONTEND_BTN_TXT[MAXJOYBUTTONS];

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

Constant::Constant(){
}

Constant::~Constant(){
}