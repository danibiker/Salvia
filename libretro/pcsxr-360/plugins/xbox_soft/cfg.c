/***************************************************************************
cfg.c  -  description
-------------------
begin                : Sun Oct 28 2001
copyright            : (C) 2001 by Pete Bernert
email                : BlackDove@addcom.de
***************************************************************************/
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version. See also the license.txt file for *
*   additional informations.                                              *
*                                                                         *
***************************************************************************/

#define _IN_CFG

#include <sys/stat.h>
#include <stdlib.h>

#undef FALSE
#undef TRUE
#define MAKELONG(low,high)     ((unsigned long)(((unsigned short)(low)) | (((unsigned long)((unsigned short)(high))) << 16)))

#include "externals.h"
#include "cfg.h"
#include "gpu.h"

#define CFG_DF_VIDEO "game:\\cfg\\dfxvideo.cfg"

char * pConfigFile = CFG_DF_VIDEO;
int darkforcesfix = 0;


void ReadConfigFile(){

}

void ExecCfg(char *arg) {

}

void SoftDlgProc(void)
{
	
}

void AboutDlgProc(void)
{

}

void ReadConfig(void)
{
 // defaults
 iResX=640;iResY=480;
 iWinSize=MAKELONG(320,240);
 iColDepth=16;
 iWindowMode=0;
 iMaintainAspect=0;
 UseFrameSkip=0;
 bInitCap = TRUE ;
 iFrameLimit=2;
 fFrameRate=200.0f;
 dwCfgFixes=darkforcesfix;
 iUseNoStretchBlt=0;
 iUseDither=0;
 iShowFPS=0;

// ReadConfigFile();

 // additional checks
 if(!iColDepth)       iColDepth=16;
 if(dwCfgFixes){ iUseFixes = 1 ; } else iUseFixes = 0 ;
 if(iUseFixes){ if (darkforcesfix){dwActFixes = 0x100;} }else dwActFixes = 0;

 SetFixes();

}

void WriteConfig(void) {

}
