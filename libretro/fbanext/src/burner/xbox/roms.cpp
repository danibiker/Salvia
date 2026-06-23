#include "burner.h"
#include <process.h>

char* gameAv = NULL;
bool avOk = false;

static unsigned ScanThreadId = 0;
static HANDLE hScanThread = NULL;
static int nOldSelect = 0;

static HXUIOBJ hRomInfoText;
static HXUIOBJ hRomProgress;
static HANDLE hEvent = NULL;

static void CreateRomDatName(TCHAR* szRomDat)
{
	_stprintf(szRomDat, _T("GAME:\\config\\FBANext.roms.dat"));

	return;
}

int RomsDirCreate(HWND hParentWND)
{
 
	return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Check Romsets Dialog/////////////////////////////////////////////////////////////////////////////

static int WriteGameAvb()
{
	TCHAR szRomDat[MAX_PATH];
	
	FILE* h;

	CreateRomDatName(szRomDat);

	if ((h = _tfopen(szRomDat, _T("wt"))) == NULL) {
		return 1;
	}

	_ftprintf(h, _T(APP_TITLE) _T(" v%.20s ROMs"), szAppBurnVer);	// identifier
	_ftprintf(h, _T(" 0x%04X "), nBurnDrvCount);					// no of games

	for (unsigned int i = 0; i < nBurnDrvCount; i++) {
		if (gameAv[i] & 2) {
			_fputtc(_T('*'), h);
		} else {
			if (gameAv[i] & 1) {
				_fputtc(_T('+'), h); 
			} else {
				_fputtc(_T('-'), h);
			}
		}
	}

	_ftprintf(h, _T(" END"));									// end marker

	fclose(h);

	return 0;
}

static int DoCheck(TCHAR* buffPos)
{
	TCHAR label[256];

	// Check identifier
	memset(label, 0, sizeof(label));
	_stprintf(label, _T(APP_TITLE) _T(" v%.20s ROMs"), szAppBurnVer);
	if ((buffPos = LabelCheck(buffPos, label)) == NULL) {
		return 1;
	}

	// Check no of supported games
	memset(label, 0, sizeof(label));
	memcpy(label, buffPos, 16);
	buffPos += 8;
	unsigned int n = _tcstol(label, NULL, 0);
	if (n != nBurnDrvCount) {
		return 1;
	}

	for (unsigned int i = 0; i < nBurnDrvCount; i++) {
		if (*buffPos == _T('*')) {
			gameAv[i] = 3;
		} else {
			if (*buffPos == _T('+')) {
				gameAv[i] = 1;
			} else {
				if (*buffPos == _T('-')) {
					gameAv[i] = 0;
				} else {
					return 1;
				}
			}
		}

		buffPos++;
	}

	memset(label, 0, sizeof(label));
	_stprintf(label, _T(" END"));
	if (LabelCheck(buffPos, label) == NULL) {
		avOk = true;
		return 0;
	} else {
		return 1;
	}
}

int CheckGameAvb()
{
	TCHAR szRomDat[MAX_PATH];
	FILE* h;
	int bOK;
	int nBufferSize = nBurnDrvCount + 256;
	TCHAR* buffer = (TCHAR*)malloc(nBufferSize * sizeof(TCHAR));
	if (buffer == NULL) {
		return 1;
	}

	memset(buffer, 0, nBufferSize * sizeof(TCHAR));
	CreateRomDatName(szRomDat);

	if ((h = _tfopen(szRomDat, _T("r"))) == NULL) {
		return 1;
	}

	_fgetts(buffer, nBufferSize, h);
	fclose(h);

	bOK = DoCheck(buffer);

	if (buffer) {
		free(buffer);
		buffer = NULL;
	}
	return bOK;
}

static int QuitRomsScan()
{
 

	BzipClose();

	nBurnDrvActive = nOldSelect;
	nOldSelect = 0;
	bRescanRoms = false;

	if (avOk) {
		WriteGameAvb();
	}

	return 1;
}

static unsigned __stdcall AnalyzingRoms(void*)
{
 
	wchar_t InfoText[255] = L"";

	for (unsigned int z = 0; z < nBurnDrvCount; z++) {
		nBurnDrvActive = z;
 
		pDevice->Clear(0, NULL, D3DCLEAR_TARGET,  D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

		if ((z % 500) == 0)
		{
			swprintf_s(InfoText,L"%d/%d Roms Scanned...", z, nBurnDrvCount );
			XuiTextElementSetText(hRomProgress,InfoText);

			// Update XUI
			app.RunFrame();
			// Render XUI
			app.Render();
 
			// Present the frame.
			pDevice->Present( NULL, NULL, NULL, NULL );
		}

		switch (BzipOpen(TRUE))	{
			case 0:
				gameAv[z] = 3;
				break;
			case 2:
				gameAv[z] = 1;
				break;
			case 1:
				gameAv[z] = 0;
		}
		BzipClose();
 
   }

	avOk = true;

	QuitRomsScan();

	return 0;
}
 

int CreateROMInfo(HXUIOBJ hParentWND)
{
 
	HXUIOBJ hBuildRomsScene = NULL;

	if (gameAv == NULL) {
		gameAv = (char*)malloc(nBurnDrvCount);
		memset(gameAv, 0, nBurnDrvCount);
	}
	
	if (gameAv) {
		if (CheckGameAvb() || bRescanRoms) {
			nOldSelect = nBurnDrvActive;
			memset(gameAv, 0, sizeof(gameAv));
 
			avOk = false;
			XuiSceneCreate( L"file://game:/media/FBANext.xzp#src\\Intf\\Skin\\", L"BuildingRoms.xur", NULL, &hBuildRomsScene );

			if (hParentWND == NULL)
				XuiSceneNavigateFirst( app.GetRootObj(), hBuildRomsScene, XUSER_INDEX_FOCUS );
			else
				XuiSceneNavigateForward( hParentWND, true, hBuildRomsScene, XUSER_INDEX_FOCUS );

			XuiElementGetChildById( hBuildRomsScene, L"XuiRomCount", &hRomProgress ); 
			AnalyzingRoms(NULL);
			XuiSceneNavigateBack(hBuildRomsScene,hParentWND,XUSER_INDEX_FOCUS);
		}
	}
	
	return 1;
}

void FreeROMInfo()
{
	if (gameAv) {
		free(gameAv);
		gameAv = NULL;
	}
}
