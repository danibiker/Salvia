// Support functions for blitters that use DirectX
#include "burner.h"
#include "vid_directx_support.h"

// ---------------------------------------------------------------------------
// General

void VidSExit()
{
	 
}



#if 0
// ----------------------------------------------------------------------------
// Gamma controls

static IDirectDrawGammaControl* pGammaControl;
static DDGAMMARAMP* pFBAGamma = NULL;
static DDGAMMARAMP* pSysGamma = NULL;

void VidSRestoreGamma()
{
	if (pGammaControl) {
		if (pSysGamma) {
			pGammaControl->SetGammaRamp(0, pSysGamma);
		}

		free(pSysGamma);
		pSysGamma = NULL;
		free(pFBAGamma);
		pFBAGamma = NULL;

		RELEASE(pGammaControl);
	}
}

int VidSUpdateGamma()
{
	if (pGammaControl) {
		if (bDoGamma) {
			for (int i = 0; i < 256; i++) {
				int nValue = (int)(65535.0 * pow((i / 255.0), nGamma));
				pFBAGamma->red[i] = nValue;
				pFBAGamma->green[i] = nValue;
				pFBAGamma->blue[i] = nValue;
			}
			pGammaControl->SetGammaRamp(0, pFBAGamma);
		} else {
			pGammaControl->SetGammaRamp(0, pSysGamma);
		}
	}

	return 0;
}

int VidSSetupGamma(IDirectDrawSurface7* pSurf)
{
	pGammaControl = NULL;

	if (!bVidUseHardwareGamma || !nVidFullscreen) {
		return 0;
	}

	if (FAILED(pSurf->QueryInterface(IID_IDirectDrawGammaControl, (void**)&pGammaControl))) {
		pGammaControl = NULL;
#ifdef PRINT_DEBUG_INFO
		dprintf(_T("  * Warning: Couldn't use hardware gamma controls.\n"));
#endif

		return 1;
	}

	pSysGamma = (DDGAMMARAMP*)malloc(sizeof(DDGAMMARAMP));
	if (pSysGamma == NULL) {
		VidSRestoreGamma();
		return 1;
	}
	pGammaControl->GetGammaRamp(0, pSysGamma);

	pFBAGamma = (DDGAMMARAMP*)malloc(sizeof(DDGAMMARAMP));
	if (pFBAGamma == NULL) {
		VidSRestoreGamma();
		return 1;
	}

	VidSUpdateGamma();

	return 0;
}
#endif

// ---------------------------------------------------------------------------
// Fullscreen mode support routines

int VidSScoreDisplayMode(VidSDisplayScoreInfo* pScoreInfo)
{
	 

	return 0;
}

int VidSInitScoreInfo(VidSDisplayScoreInfo* pScoreInfo)
{
 
	return 0;
}
 
void VidSRestoreScreenMode()
{
	 
}

// Enter fullscreen mode, select optimal full-screen resolution
int VidSEnterFullscreenMode(int nZoom, int nDepth)
{
	 

	return 0;
}

// ---------------------------------------------------------------------------
// Text display routines

#define MESSAGE_SIZE 64
// short message
static struct {
	TCHAR pMsgText[MESSAGE_SIZE];
	COLORREF nColour;
	int nPriority;
	unsigned int nTimer;
} VidSShortMsg = { _T(""), 0, 0, 0,};
static HFONT ShortMsgFont = NULL;
const int shortMsgWidth = 240;

// tiny message
static struct {
	TCHAR pMsgText[MESSAGE_SIZE];
	COLORREF nColour;
	int nPriority;
	unsigned int nTimer;
} VidSTinyMsg = {_T(""), 0, 0, 0};
static HFONT TinyMsgFont = NULL;
const int tinyMsgWidth = 300;

// chat message
#define CHAT_SIZE 11
static struct {
	TCHAR* pIDText;
	COLORREF nIDColour;
	TCHAR* pMainText;
	COLORREF nMainColour;
} VidSChatMessage[CHAT_SIZE];

static bool bChatInitialised = false;
static HFONT ChatIDFont = NULL;
static HFONT ChatMainFont = NULL;
static unsigned int nChatTimer;
static int nChatFontSize;
static int nChatShadowOffset;
static int nChatOverFlow;
static bool bDrawChat;

static HFONT EditTextFont = NULL;
static HFONT EditCursorFont = NULL;
static DWORD nPrevStart, nPrevEnd;
static int nCursorTime;
static int nCursorState = 0;
static int nEditSize;
static int nEditShadowOffset;
 
static unsigned int nKeyColour = 0x000001;

int nMaxChatFontSize = 20;		// Maximum size of the font used for the Kaillera chat function (1280x960 or higher)
int nMinChatFontSize = 12;		// Minimum size of the font used for the Kaillera chat function (arcade resolution)

static int nZoom;				// & 1: zoom X, & 2: zoom Y

static int nShortMsgFlags;

bool bEditActive = false;
bool bEditTextChanged = false;
TCHAR EditText[MAX_CHAT_SIZE + 1] = _T("");
 
unsigned int nOSDTimer = 0;
TCHAR tinyMsg[64] = _T("");
unsigned int nTinyTimer = 0;
 

static void VidSExitTinyMsg()
{
 
}

static void VidSExitShortMsg()
{
 
}

void VidSExitChat()
{
	 
}

static void VidSExitEdit()
{
 
}

void VidSExitOSD()
{
 
}

static int VidSInitTinyMsg(int nFlags)
{
	 

	return 0;
}

static int VidSInitShortMsg(int nFlags)
{
	 
	 
	return 0;
}

static int VidSInitChat(int nFlags)
{
 
	return 0;
}

static int VidSInitEdit(int nFlags)
{
 

	return 0;
}

int VidSInitOSD(int nFlags)
{ 
	return 0;
}

int VidSRestoreOSD()
{
 

	return 0;
}

 

static int VidSDrawChat(RECT* dest)
{
 
	return 0;
}
 

int VidSNewTinyMsg(const TCHAR* pText, int nRGB, int nDuration, int nPriority)	// int nRGB = 0, int nDuration = 0, int nPriority = 5
{
	 
	return 0;
 }

int VidSNewShortMsg(const TCHAR* pText, int nRGB, int nDuration, int nPriority)	// int nRGB = 0, int nDuration = 0, int nPriority = 5
{
 
	return 0;
}

void VidSKillShortMsg()
{
	VidSShortMsg.nTimer = 0;
}

void VidSKillTinyMsg()
{
	VidSTinyMsg.nTimer = 0;
}

void VidSKillOSDMsg()
{
	nOSDTimer = 0;
}

void VidSKillTinyOSDMsg()
{
	nTinyTimer = 0;
}

int VidSAddChatMsg(const TCHAR* pID, int nIDRGB, const TCHAR* pMain, int nMainRGB)
{
 
	return 0;
}


// ---------------------------------------------------------------------------

// Compute the resolution needed to run a game without scaling and filling the screen
// (taking into account the aspect ratios of the game and monitor)
bool VidSGetArcaderes(int* pWidth, int* pHeight)
{
	int nGameWidth, nGameHeight;
	int nGameAspectX, nGameAspectY;

	if (bDrvOkay) {
		if ((BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) && (nVidRotationAdjust & 1)) {
			BurnDrvGetVisibleSize(&nGameHeight, &nGameWidth);
			BurnDrvGetAspect(&nGameAspectY, &nGameAspectX);
		} else {
			BurnDrvGetVisibleSize(&nGameWidth, &nGameHeight);
			BurnDrvGetAspect(&nGameAspectX, &nGameAspectY);
		}

		double dMonAspect = (double)nVidScrnAspectX / nVidScrnAspectY;
		double dGameAspect = (double)nGameAspectX / nGameAspectY;

		if (dMonAspect > dGameAspect) {
			*pWidth = nGameHeight * nGameAspectY * nGameWidth * nVidScrnAspectX / (nGameHeight * nGameAspectX * nVidScrnAspectY);
			*pHeight = nGameHeight;
		} else {
			*pWidth = nGameWidth;
			*pHeight = nGameWidth * nGameAspectX * nGameHeight * nVidScrnAspectY / (nGameWidth * nGameAspectY * nVidScrnAspectX);
		}

		// Horizontal resolution must be a multiple of 8
		if (*pWidth - nGameWidth < 8) {
			*pWidth = (*pWidth + 7) & ~7;
		} else {
			*pWidth = (*pWidth + 4) & ~7;
		}
	} else {
		return false;
	}

	return true;
}

// This function takes a rectangle and scales it to either:
// - The largest possible multiple of both X and Y;
// - The largest possible multiple of Y, modifying X to ensure the correct aspect ratio;
// - The window size
int VidSScaleImage(RECT* pRect, int nGameWidth, int nGameHeight, bool bVertScanlines)
{
	int xm, ym;											// The multiple of nScrnWidth and nScrnHeight we can fit in
	int nScrnWidth, nScrnHeight;

	int nGameAspectX = 4, nGameAspectY = 3;
	int nWidth = pRect->right - pRect->left;
	int nHeight = pRect->bottom - pRect->top;

	if (bVidFullStretch) {								// Arbitrary stretch
		return 0;
	}

	if (bDrvOkay) {
		if ((BurnDrvGetFlags() & (BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED)) && (nVidRotationAdjust & 1)) {
			BurnDrvGetAspect(&nGameAspectY, &nGameAspectX);
		} else {
			BurnDrvGetAspect(&nGameAspectX, &nGameAspectY);
		}
	}

	xm = nWidth / nGameWidth;
	ym = nHeight / nGameHeight;

	if (nVidFullscreen) {
		nScrnWidth = nVidScrnWidth;
		nScrnHeight = nVidScrnHeight;
	} else {
		nScrnWidth = SystemWorkArea.right - SystemWorkArea.left;
		nScrnHeight = SystemWorkArea.bottom - SystemWorkArea.top;
	}

	if (bVidCorrectAspect && bVidScanlines && ((ym >= 2 && xm) || (ym && xm >= 2 && bVertScanlines))) {	// Correct aspect ratio with scanlines
		int nWidthScratch, nHeightScratch;
		if (nGameWidth < nGameHeight && bVertScanlines) {
			int xmScratch = xm;
			do {
				nWidthScratch = nGameWidth * xmScratch;
				nHeightScratch = nWidthScratch * nVidScrnAspectX * nGameAspectY * nScrnHeight / (nScrnWidth * nVidScrnAspectY * nGameAspectX);
				xmScratch--;
			} while (nHeightScratch > nHeight && xmScratch >= 2);
			if (nHeightScratch > nHeight) {				// The image is too high
				if (nGameWidth < nGameHeight) {			// Vertical games
					nWidth = nHeight * nVidScrnAspectX * nGameAspectX * nScrnHeight / (nScrnWidth * nVidScrnAspectY * nGameAspectY);
				} else {								// Horizontal games
					nWidth = nHeight * nVidScrnAspectY * nGameAspectX * nScrnWidth / (nScrnHeight * nVidScrnAspectX * nGameAspectY);
				}
			} else {
				nWidth = nWidthScratch;
				nHeight = nHeightScratch;
			}
		} else {
			int ymScratch = ym;
			do {
				nHeightScratch = nGameHeight * ymScratch;
				nWidthScratch = nHeightScratch * nVidScrnAspectY * nGameAspectX * nScrnWidth / (nScrnHeight * nVidScrnAspectX * nGameAspectY);
				ymScratch--;
			} while (nWidthScratch > nWidth && ymScratch >= 2);
			if (nWidthScratch > nWidth) {				// The image is too wide
				if (nGameWidth < nGameHeight) {			// Vertical games
					nHeight = nWidth * nVidScrnAspectY * nGameAspectY * nScrnWidth / (nScrnHeight * nVidScrnAspectX * nGameAspectX);
				} else {								// Horizontal games
					nHeight = nWidth * nVidScrnAspectX * nGameAspectY * nScrnHeight / (nScrnWidth * nVidScrnAspectY * nGameAspectX);
				}
			} else {
				nWidth = nWidthScratch;
				nHeight = nHeightScratch;
			}
		}
	} else {
		if (bVidCorrectAspect) {					// Correct aspect ratio
			int nWidthScratch;
			nWidthScratch = nHeight * nVidScrnAspectY * nGameAspectX * nScrnWidth / (nScrnHeight * nVidScrnAspectX * nGameAspectY);
			if (nWidthScratch > nWidth) {			// The image is too wide
				if (nGameWidth < nGameHeight) {		// Vertical games
					nHeight = nWidth * nVidScrnAspectY * nGameAspectY * nScrnWidth / (nScrnHeight * nVidScrnAspectX * nGameAspectX);
				} else {							// Horizontal games
					nHeight = nWidth * nVidScrnAspectX * nGameAspectY * nScrnHeight / (nScrnWidth * nVidScrnAspectY * nGameAspectX);
				}
			} else {
				nWidth = nWidthScratch;
			}
		} else {
			if (xm && ym) {							// Don't correct aspect ratio
				if (xm > ym) {
					xm = ym;
				} else {
					ym = xm;
				}
				nWidth = nGameWidth * xm;
				nHeight = nGameHeight * ym;
			} else {
				if (xm) {
					nWidth = nGameWidth * xm * nHeight / nGameHeight;
				} else {
					if (ym) {
						nHeight = nGameHeight * ym * nWidth / nGameWidth;
					}
				}
			}
		}
	}

	pRect->left = (pRect->right + pRect->left) / 2;
	pRect->left -= nWidth / 2;
	pRect->right = pRect->left + nWidth;

	pRect->top = (pRect->top + pRect->bottom) / 2;
	pRect->top -= nHeight / 2;
	pRect->bottom = pRect->top + nHeight;

	return 0;
}
