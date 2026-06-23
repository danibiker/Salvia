// Media module
#include "burner.h"

int MediaInit()
{
	if (ScrnInit()) {					// Init the Scrn Window
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_UI_WINDOW));
		FBAPopupDisplay(PUF_TYPE_ERROR);
		return 1;
	}

	if (!bInputOkay) {
		InputInit();					// Init Input
	}

	nAppVirtualFps = nBurnFPS;

	if (!bAudOkay) {
		AudSoundInit();					// Init Sound (not critical if it fails)
	}

	nBurnSoundRate = 0;					// Assume no sound
	pBurnSoundOut = NULL;
	if (bAudOkay) {
		nBurnSoundRate = nAudSampleRate[nAudSelect];
		nBurnSoundLen = nAudSegLen;
	}

	if (!bVidOkay) {

		// Reinit the video plugin
		VidInit();
		if (!bVidOkay && nVidFullscreen) {

			nVidFullscreen = 0;

			MediaExit();
			return (MediaInit());
		}
		if (!nVidFullscreen) {
			ScrnSize();
		}

		if (!bVidOkay) {
			// Make sure the error will be visible			 
			FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_UI_MODULE), VidGetModuleName());
			FBAPopupDisplay(PUF_TYPE_ERROR);
		}

		if (bVidOkay && ((bRunPause && bAltPause) || !bDrvOkay)) {
			VidRedraw();
		}
	}

	return 0;
}

int MediaExit()
{
	nBurnSoundRate = 0;					// Blank sound
	pBurnSoundOut = NULL;

	AudSoundExit();						// Exit Dsound

	VidExit();

	InputExit();
 
	ScrnExit();							// Exit the Scrn Window

	return 0;
}
