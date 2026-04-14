/***************************************************************************
                           StdAfx.h  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by Pete Bernert
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

#if defined(_WINDOWS) || defined(_XBOX)

#ifdef _XBOX
#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <xtl.h>
#include <process.h>
#include <stdlib.h>

#ifndef INLINE
#define INLINE __inline
#endif

#pragma warning (disable:4996)
#else
#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>
#include <windowsx.h>
#include "mmsystem.h"
#include <process.h>
#include <stdlib.h>

#ifndef INLINE
#define INLINE __inline
#endif

#include "resource.h"

#pragma warning (disable:4996)
#endif
#else

#ifndef _MACOSX
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef USEOSS
#include <sys/soundcard.h>
#endif
#include <unistd.h>
#include <pthread.h>
#define RRand(range) (random()%range)  
#include <string.h> 
#include <sys/time.h>  
#include <math.h>  

#undef CALLBACK
#define CALLBACK
#define DWORD unsigned int
#define LOWORD(l)           ((unsigned short)(l)) 
#define HIWORD(l)           ((unsigned short)(((unsigned int)(l) >> 16) & 0xFFFF)) 

#ifndef INLINE
#define INLINE inline
#endif

#endif

#if defined (__GNUC__) || defined (__clang__)
#define UNUSED_VARIABLE __attribute__((unused))
#else
#define UNUSED_VARIABLE
#endif

#include "psemuxa.h"

#ifdef _XBOX
#define SPUreadDMA				PEOPS_SPUreadDMA
#define SPUreadDMAMem			PEOPS_SPUreadDMAMem
#define SPUwriteDMA				PEOPS_SPUwriteDMA
#define SPUwriteDMAMem			PEOPS_SPUwriteDMAMem
#define SPUasync				PEOPS_SPUasync
#define SPUupdate				PEOPS_SPUupdate
#define SPUplayADPCMchannel		PEOPS_SPUplayADPCMchannel
#define SPUinit					PEOPS_SPUinit
#define SPUopen					PEOPS_SPUopen
#define SPUsetConfigFile		PEOPS_SPUsetConfigFile
#define SPUclose				PEOPS_SPUclose
#define SPUshutdown				PEOPS_SPUshutdown
#define SPUtest					PEOPS_SPUtest
#define SPUconfigure			PEOPS_SPUconfigure
#define SPUabout				PEOPS_SPUabout
#define SPUregisterCallback		PEOPS_SPUregisterCallback
#define SPUregisterCDDAVolume	PEOPS_SPUregisterCDDAVolume
#define SPUplayCDDAchannel		PEOPS_SPUplayCDDAchannel
#define SPUwriteRegister		PEOPS_SPUwriteRegister
#define SPUreadRegister			PEOPS_SPUreadRegister
#define SPUfreeze				PEOPS_SPUfreeze

#define ReadConfig				SPUReadConfig
#define ReadConfigFile			SPUReadConfigFile
#define WriteConfig				SPUWriteConfig
#define iDebugMode				SPUiDebugMode

#define SWAP16(x) _byteswap_ushort(x)
#else
#endif