 
#include "burner.h" 
//#include "vid_directx_support.h"
#include "InGameOptions.h"
 
TCHAR szPrevGames[SHOW_PREV_GAMES][32];

RECT SystemWorkArea = { 0, 0, 1280, 720 };		// Work area on the desktop
int nWindowPosX = -1, nWindowPosY = -1;			// Window position

int bAutoPause = 0;
int nSavestateSlot = 1;

int bOnce = 0;
bool bShowOnTop = false;
bool bFullscreenOnStart = false;
bool bFakeFullscreen = false;
bool bRescanRoms = false;

static TCHAR* szClass = _T(APP_TITLE);			// Window class name
HWND hScrnWnd = NULL;							// Handle to the screen window
HWND hVideoWnd = NULL;							// Handle to the video window

static bool bMaximised;
static int nPrevWidth, nPrevHeight;

int nWindowSize = 0;

static bool bDrag = false;
static int nDragX, nDragY;
static int nOldWindowX, nOldWindowY;
static int nLeftButtonX, nLeftButtonY;

extern void dx9SetTargets();
extern void doResetDX9();
extern HXUIOBJ hInGameOptionsScene;

bool useDialogs()
{ 
	return false;
}
 
void SetPauseMode(bool bPause)
{
	bRunPause = bPause;
	bAltPause = bPause;

	if (bPause) {

		AudBlankSound();		

		XUIMessage xuiMsg;
		InGameMenuStruct msgData;
		InGameMenuFirstFunc( &xuiMsg, &msgData, 1 );
		XuiSendMessage( hInGameOptionsScene, &xuiMsg );

		bOnce = 1;

	} else {
		GameInpCheckMouse();
	}

	

}
 

int CreateDatfileWindows(int bType)
{
	TCHAR szTitle[1024];
	TCHAR szFilter[1024];
	
	TCHAR szConsoleString[64];
	_sntprintf(szConsoleString, 64, _T(""));
	if (bType == DAT_MEGADRIVE_ONLY) _sntprintf(szConsoleString, 64, _T(", Megadrive only"));
	if (bType == DAT_PCENGINE_ONLY) _sntprintf(szConsoleString, 64, _T(", PC-Engine only"));
	if (bType == DAT_TG16_ONLY) _sntprintf(szConsoleString, 64, _T(", TurboGrafx16 only"));
	if (bType == DAT_SGX_ONLY) _sntprintf(szConsoleString, 64, _T(", SuprGrafx only"));
	
	TCHAR szProgramString[25];	
	_sntprintf(szProgramString, 25, _T("ClrMame Pro XML"));
	
	_sntprintf(szChoice, MAX_PATH, _T("GAME:\\") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), szAppBurnVer, szProgramString, szConsoleString);
 

	return create_datfile(szChoice, bType);
}

// Returns true if a VidInit is needed when the window is resized
static inline bool VidInitNeeded()
{
	return true;
}

 
bool bLoading = false;
 
static int ScrnRegister()
{
 
	return 0;
}

int ScrnTitle()
{
 
	return 0;
}

int ScrnSize()
{
 

  	return 0;
}

// Init the screen window (create it)
int ScrnInit()
{
 

	return 0;
}

// Exit the screen window (destroy it)
int ScrnExit()
{
 

	return 0;
}

// ==> switch fullscreen, added by regret
int ScrnSwitchFull()
{
 
	return 0;
}

// fake fullscreen
int ScrnFakeFullscreen()
{
 

	return 0;
}

// set fullscreen (for opengl)
int ScrnSetFull(const bool& full)
{ 
	return 0;
} 

void Reinitialise()
{
	POST_INITIALISE_MESSAGE;
	bRestartVideo=1;
	//VidReInitialise();
}