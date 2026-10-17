/**
 * glN64_GX - SoftGFXPlugin.h 
 * Copyright (C) 2007 Mike Slegeir
 * Mangles the plugin so it can be linked staticly
 *
 * Wii64 homepage: http://www.emulatemii.com
 *
**/

#ifndef GFX_PLUGIN_H
#define GFX_PLUGIN_H

#define RomClosed  romClosed_gfx
#define RomOpen    romOpen_gfx
#define GetDllInfo getDllInfo_gfx
#define DllConfig  dllConfig_gfx
#define DllTest    dllTest_gfx
#define DllAbout   dllAbout_gfx
#define CloseDLL   closeDLL_gfx

#define CaptureScreen   captureScreen
#define ChangeWindow    changeWindow
#define DrawScreen      drawScreen
#define InitiateGFX     initiateGFX
#define MoveScreen      moveScreen
#define ProcessDList    processDList
#define ProcessRDPList  processRDPList
#define ShowCFB         showCFB
#define UpdateScreen    updateScreen
#define ViStatusChanged viStatusChanged
#define ViWidthChanged  viWidthChanged

/* ★ DIVERGENCIA vs Wii64: esta linea la anadimos nosotros.
 * Ver xenon/gxshim/DIVERGENCIAS.txt punto 10.
 *
 * Upstream renombra 18 de los 19 puntos de entrada que exporta glN64.cpp y
 * se dejo `ReadScreen` fuera de la lista.  En Wii64 daba igual porque alli
 * glN64 es el UNICO plugin grafico; aqui convive con Rice, que tambien
 * define `ReadScreen` (Video.cpp:1465), y el enlazador saca
 *
 *     Video.obj : warning LNK4006: ReadScreen already defined in glN64.obj;
 *                 second definition ignored
 *
 * Es solo un WARNING, y por eso es peligroso: el enlazador se queda con UNA
 * de las dos en silencio.  Hoy no revienta porque `gfx.readScreen` no se
 * asigna nunca (queda a NULL por el `= {NULL}` de plugin.c:50), pero el dia
 * que se cableara llamaria al plugin equivocado sin avisar.
 *
 * Comprobado que es la UNICA que falta: de los 19 simbolos EXPORT de
 * glN64.cpp, los otros 18 si estan en esta tabla. */
#define ReadScreen      readScreen_gfx

#endif

