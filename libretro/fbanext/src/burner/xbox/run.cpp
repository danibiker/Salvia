// Run module
#include "burner.h"

int bRunPause = 0;
int bAltPause = 0;

int bAlwaysDrawFrames = 0;
int bRestartVideo = 0;

static bool bShowFPS = false;
static unsigned int nDoFPS = 0;

static bool bMute = false;
static int nOldAudVolume;

int kNetGame = 0;							// Non-zero if Kaillera is being used

#ifdef FBA_DEBUG
int counter;								// General purpose variable used when debugging
#endif

static unsigned int nNormalLast = 0;		// Last value of timeGetTime()
static int nNormalFrac = 0;					// Extra fraction we did

static bool bAppDoStep = 0;
static bool bAppDoFast = 0;
static int nFastSpeed = 6;

extern bool exitGame;
extern int RenderMap(void);

static int GetInput(bool bCopy)
{
	static int i = 0;
	InputMake(bCopy); 						// get input
 
	InpdUpdate();

	// Update Input Set dialog
	InpsUpdate();
	return 0;
}

static void DisplayFPS()
{
	static time_t fpstimer;
	static unsigned int nPreviousFrames;

	TCHAR fpsstring[8];
	time_t temptime = clock();
	double fps = (double)(nFramesRendered - nPreviousFrames) * CLOCKS_PER_SEC / (temptime - fpstimer);
	_sntprintf(fpsstring, 7, _T("%2.2lf"), fps);
	VidSNewShortMsg(fpsstring, 0xDFDFFF, 480, 0);

	fpstimer = temptime;
	nPreviousFrames = nFramesRendered;
}

// define this function somewhere above RunMessageLoop()
void ToggleLayer(unsigned char thisLayer)
{
	nBurnLayer ^= thisLayer;				// xor with thisLayer
	VidRedraw();
	VidPaint(0);
}

// With or without sound, run one frame.
// If bDraw is true, it's the last frame before we are up to date, and so we should draw the screen
static int RunFrame(int bDraw, int bPause)
{
	static int bPrevPause = 0;
	static int bPrevDraw = 0;

	if (bPrevDraw && !bPause) {
		VidPaint(0);							// paint the screen (no need to validate)
	}

	if (!bDrvOkay) {
		return 1;
	}


	if (bPause) {
		
		AudBlankSound();
		GetInput(false);						// Update burner inputs, but not game inputs


		if (bPause != bPrevPause) {
			VidPaint(2);                        // Redraw the screen (to ensure mode indicators are updated)
		}
		
#ifndef _DEBUG
		RenderMap();
#endif
	 		
		// Update XUI
		app.RunFrame();
		// Render XUI
		app.Render();
		// Update XUI Timers
		XuiTimersRun();

		pDevice->Present(NULL, NULL, NULL, NULL);
	 
	} else {

		nFramesEmulated++;
		nCurrentFrame++;

		GetInput(true);					// Update inputs
 

		if (bDraw) {
			nFramesRendered++;

			if (VidFrame()) {					// Do one frame
				AudBlankSound();
			}
		} else {								// frame skipping
			pBurnDraw = NULL;					// Make sure no image is drawn
			BurnDrvFrame();
		}

		if (bShowFPS) {
			if (nDoFPS < nFramesRendered) {
				DisplayFPS();
				nDoFPS = nFramesRendered + 30;
			}
		}
	}

	bPrevPause = bPause;
	bPrevDraw = bDraw;

	return 0;
}

// Callback used when DSound needs more sound
static int RunGetNextSound(int bDraw)
{
	if (nAudNextSound == NULL) {
		return 1;
	}

	if (bRunPause) {
		if (bAppDoStep) {
			RunFrame(bDraw, 0);
			memset(nAudNextSound, 0, nAudSegLen << 2);	// Write silence into the buffer
		} else {
			RunFrame(bDraw, 1);
		}

		bAppDoStep = 0;									// done one step
		return 0;
	}

	if (bAppDoFast) {									// do more frames
		for (int i = 0; i < nFastSpeed; i++) {
			RunFrame(0, 0);
		}
	}

	// Render frame with sound
	pBurnSoundOut = nAudNextSound;
	RunFrame(bDraw, 0);

	if (bAppDoStep) {
		memset(nAudNextSound, 0, nAudSegLen << 2);		// Write silence into the buffer
	}
	bAppDoStep = 0;										// done one step

	return 0;
}

extern bool bRunFrame;

int RunIdle()
{
	int nTime, nCount;

	if (bAudPlaying) {
		// Run with sound
		
		if(bRunFrame) {
			bRunFrame = false;
			if(bAlwaysDrawFrames) {
				RunFrame(1, 0);
			} else {
				RunGetNextSound(0);
			}
			return 0;
		} else {
			AudSoundCheck();
			return 0;
		}
	}

	// Run without sound
	nTime = GetTickCount() - nNormalLast;
	nCount = (nTime * nAppVirtualFps - nNormalFrac) / 100000;
	if (nCount <= 0) {						// No need to do anything for a bit
		//Sleep(2);
		return 0;
	}

	nNormalFrac += nCount * 100000;
	nNormalLast += nNormalFrac / nAppVirtualFps;
	nNormalFrac %= nAppVirtualFps;

	//if (bAppDoFast){						// Temporarily increase virtual fps
	//	nCount *= nFastSpeed;
	//}
	if (nCount > 100) {						// Limit frame skipping
		nCount = 100;
	}
	if (bRunPause) {
		if (bAppDoStep) {					// Step one frame
			nCount = 10;
		} else {
			RunFrame(1, 1);					// Paused
			return 0;
		}
	}
	bAppDoStep = 0;

	if (bAppDoFast) {									// do more frames
		for (int i = 0; i < nFastSpeed; i++) {
			RunFrame(0, 0);
		}
	}

	if(!bAlwaysDrawFrames) {
		for (int i = nCount / 10; i > 0; i--) {	// Mid-frames
			RunFrame(0, 0);
		}
	}
	RunFrame(1, 0);							// End-frame

	return 0;
}

int RunReset()
{
	// Reset the speed throttling code
	nNormalLast = 0; nNormalFrac = 0;
	// Reset FPS display
	nDoFPS = 0;

	if (!bAudPlaying) {
		// run without sound
		nNormalLast = GetTickCount();
	}
	return 0;
}

static int RunInit()
{
	// Try to run with sound
	AudSetCallback(RunGetNextSound);
	AudSoundPlay();

	RunReset();

	return 0;
}

static int RunExit()
{
	nNormalLast = 0;
	// Stop sound if it was playing
	AudSoundStop();
	return 0;
}

 
int RunMessageLoop()
{	
	int once = 0;
 
	bRestartVideo = 0;
 
	RunInit();
 
	GameInpCheckLeftAlt();
	GameInpCheckMouse();								// Hide the cursor

	while (!exitGame) { 

		if (bRestartVideo) {
			VidExit();
			MediaInit();			
			bRestartVideo = 0;
		}
		
		RunIdle();	

	}

	RunExit();				 		 
	MediaExit();
	return 0;
}