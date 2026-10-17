/**
 * Wii64 - wii64config.h
 * Copyright (C) 2007, 2008, 2009, 2013 sepp256
 * 
 * External declaration and enumeration of config variables
 *
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
**/


#ifndef WII64CONFIG_H
#define WII64CONFIG_H

#include "timers.h"

/* [Xbox360] ★ ENLACE C, QUE EL ORIGINAL NO DECLARA.
 *
 * Esta cabecera la ven CUATRO ficheros C (gamehacks.c, rom_gc.c,
 * savestates_gc.c y xbox/plataforma.c) y DOS C++ (glN64_GX/VI.cpp y
 * xenon/gln64_glue.cpp).  Sin `extern "C"`, cada lado usa un simbolo
 * distinto para el mismo nombre y salen DOS VARIABLES.
 *
 * ★★ Y el fallo no da ni un aviso del enlazador -- son simbolos
 * distintos, no duplicados -- asi que el sintoma es "la opcion no hace
 * nada".  Ya mordio una vez con `enablegDPUpdateColorImage`: lo escribe
 * `rom_gc.c:183` por CRC de la ROM y lo leia `gDP.cpp` de su propia
 * copia, o sea que el arreglo del cartel de Mario Kart 64 no llegaba.
 *
 * Con esto todos quedan en enlace C, y entonces los duplicados SI son
 * duplicados de verdad: cada global se define en UN solo sitio. */
#ifdef __cplusplus
extern "C" {
#endif

extern char miniMenuActive;
enum miniMenuActive
{
	MINIMENU_DISABLE=0,
	MINIMENU_ENABLE
};

extern char audioEnabled;
enum audioEnabled
{
	AUDIO_DISABLE=0,
	AUDIO_ENABLE
};

extern char showFPSonScreen;
enum showFPSonScreen
{
	FPS_HIDE=0,
	FPS_SHOW
};

extern char printToScreen;
enum printToScreen
{
	DEBUG_HIDE=0,
	DEBUG_SHOW
};

extern char printToSD;
enum printToSD
{
	SDLOG_DISABLE=0,
	SDLOG_ENABLE
};

extern timers Timers;			//Timers.limitVIs: 
enum limitVIs
{
	LIMITVIS_NONE=0,
	LIMITVIS_WAIT_FOR_VI,
	LIMITVIS_WAIT_FOR_FRAME
};

extern char saveEnabled;	//???

extern char nativeSaveDevice;
enum nativeSaveDevice
{
	NATIVESAVEDEVICE_NONE=-1,
	NATIVESAVEDEVICE_SD,
	NATIVESAVEDEVICE_USB
};

extern char saveStateDevice;
enum saveStateDevice
{
	SAVESTATEDEVICE_SD=0,
	SAVESTATEDEVICE_USB
};

extern char autoSave; //TODO: Use me
enum autoSave
{
	AUTOSAVE_DISABLE=0,
	AUTOSAVE_ENABLE
};

extern char creditsScrolling;	//deprecated?

extern unsigned long dynacore;
enum dynacore
{
	DYNACORE_INTERPRETER=0,
	DYNACORE_DYNAREC,
	DYNACORE_PURE_INTERP
};

extern unsigned long count_per_op;
enum count_per_op
{
	COUNT_PER_OP_1=1,
	COUNT_PER_OP_2,
	COUNT_PER_OP_3
};

extern char screenMode;
enum screenMode
{
	SCREENMODE_4x3=0,
	SCREENMODE_16x9,
	SCREENMODE_16x9_PILLARBOX
};

extern char videoMode;
enum videoMode
{
	VIDEOMODE_AUTO=0,
	VIDEOMODE_PAL60,
	VIDEOMODE_240P,
	VIDEOMODE_480P,
	VIDEOMODE_PAL,
	VIDEOMODE_288P,
	VIDEOMODE_576P
};

extern char padAutoAssign;
enum padAutoAssign
{
	PADAUTOASSIGN_MANUAL=0,
	PADAUTOASSIGN_AUTOMATIC
};

extern char padType[4];
enum padType
{
	PADTYPE_NONE=0,
	PADTYPE_GAMECUBE,
	PADTYPE_WII
};

extern char padAssign[4];
enum padAssign
{
	PADASSIGN_INPUT0=0,
	PADASSIGN_INPUT1,
	PADASSIGN_INPUT2,
	PADASSIGN_INPUT3
};

extern char pakMode[4];
enum pakMode
{
	PAKMODE_MEMPAK=0,
	PAKMODE_RUMBLEPAK
};

extern char loadButtonSlot;
enum loadButtonSlot
{
	LOADBUTTON_SLOT0=0,
	LOADBUTTON_SLOT1,
	LOADBUTTON_SLOT2,
	LOADBUTTON_SLOT3,
	LOADBUTTON_DEFAULT
};


//#ifdef GLN64_GX
extern char glN64_useFrameBufferTextures;
enum glN64_useFrameBufferTextures
{
	GLN64_FBTEX_DISABLE=0,
	GLN64_FBTEX_ENABLE
};

extern char glN64_use2xSaiTextures;
enum glN64_use2xSaiTextures
{
	GLN64_2XSAI_DISABLE=0,
	GLN64_2XSAI_ENABLE
};

extern char renderCpuFramebuffer;
enum renderCpuFramebuffer
{
	CPUFRAMEBUFFER_DISABLE=0,
	CPUFRAMEBUFFER_ENABLE
};
//#endif //GLN64_GX

extern char nativeOutput;
enum ConfigNativeOutput
{
	NATIVEOUT_DISABLE=0,
	NATIVEOUT_ENABLE
};

extern char enablegDPUpdateColorImage;

#ifdef __cplusplus
}
#endif

#endif //WII64CONFIG_H
