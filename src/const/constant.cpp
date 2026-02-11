#include "constant.h"

std::string Constant::appDir;
std::string Constant::appExecutable;
char Constant::tempFileSep[2];

const char *MEDIAS_TO_FIND[] = {"sstitle", "ss", "box-2D"};
const char *ASSETS_DIR[] = {"snaptit", "snap", "box2d", "synopsis"};


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
	"screenshot.png"
};

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

const char* FRONTEND_BTN_TXT[] = {"Arriba","Abajo","Izquierda","Derecha","Aceptar","Cancelar", "X", "Y", 
		"Pagina anterior", "Pagina siguiente", "Select", "Start", "Click Stick Izquierdo", "Click Stick Derecho"};

const char *ICONS_CARTS_PATH[] = {"Nintendo - Game Boy Advance-content.png",
	"Nintendo - Game Boy-content.png",
	"Sega - Master System - Mark III-content.png",
	"Sega - Mega Drive - Genesis-content.png",
	"Nintendo - Super Nintendo Entertainment System-content.png",
	"Sega - 32X-content.png",
	"Sega - Game Gear-content.png",
	"Sega - Mega-CD - Sega CD-content.png",
	"Nintendo - Nintendo Entertainment System-content.png",
	"NEC - PC Engine - TurboGrafx 16-content.png",
	"Sony - PlayStation-content.png"
};

Constant::Constant(){
}

Constant::~Constant(){
}