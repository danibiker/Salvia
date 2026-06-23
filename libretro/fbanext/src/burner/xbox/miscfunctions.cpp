// misc win32 functions
#include "burner.h"

int nScreenSize = 0;
int nScreenSizeHor = 0;
int nScreenSizeVer = 0;

// Set the current directory to be the application's directory
int appDirectory()
{
	return 0;
}
 
// ---------------------------------------------------------------------------

// Get the position of the client area of a window on the screen
int GetClientScreenRect(HWND hWnd, RECT *pRect)
{
	POINT Corner = {0, 0};

	pRect->left = 0;
	pRect->right = nVidScrnWidth;
	pRect->top = 0;
	pRect->bottom = nVidScrnHeight;

	return 0;
}

// Put a window in the middle of another window
int wndInMid(HWND hMid, HWND hBase)
{ 
	return 0;
}
 
void createNeedDir()
{
	CreateDirectory("game:\\roms", NULL);
	CreateDirectory("game:\\roms\\neocdz", NULL);
	CreateDirectory("game:\\config", NULL);
	CreateDirectory("game:\\config\\games", NULL);
	CreateDirectory("game:\\config\\presets",NULL);
	CreateDirectory("game:\\previews", NULL);
	CreateDirectory("game:\\previews\\neocdz", NULL);
	CreateDirectory("game:\\cheats", NULL);
	CreateDirectory("game:\\screenshots", NULL);
	CreateDirectory("game:\\savestates", NULL);
	CreateDirectory("game:\\recordings", NULL);
	CreateDirectory("game:\\skins", NULL);
	CreateDirectory("game:\\ips", NULL);
	CreateDirectory("game:\\titles", NULL);
	CreateDirectory("game:\\flyers", NULL);
	CreateDirectory("game:\\scores", NULL);
	CreateDirectory("game:\\selects", NULL);
	CreateDirectory("game:\\gameovers", NULL);
	CreateDirectory("game:\\bosses", NULL);
	CreateDirectory("game:\\icons", NULL);
	CreateDirectory("game:\\videos", NULL);
	CreateDirectory("game:\\samples", NULL);
	CreateDirectory("game:\\shaders", NULL);
	
 
}

bool directoryExists(const TCHAR* dirname)
{
	if (!dirname) {
		return false;
	}

	DWORD res = GetFileAttributes(dirname);
	return res != 0xFFFFFFFF && res & FILE_ATTRIBUTE_DIRECTORY;
}

 
 
// ---------------------------------------------------------------------------
// For DAT files printing and Kaillera windows

char* decorateGameName(unsigned int drv)
{
	 return _T("");
}


int ProgressCreate()
{
 

	return 0;
}

int ProgressDestroy()
{
 

	return 0;
}

 