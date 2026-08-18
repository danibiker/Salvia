#pragma once

#include <const/constant.h>

static const enum videoScale { FULLSCREEN=0, SCALE1X, SCALE2X, SCALE2X_ADV,
	SCALE_HQ2X_ALT, 
	SCALE_XBRZ_2X, SCALE_XBRZ_2X_TH, SCALE3X, SCALE3X_ADV, 
	SCALE_HQ3X_ALT, 
	SCALE_XBRZ_3X, SCALE_XBRZ_3X_TH, SCALE4X, 
	SCALE4X_ADV, SCALE_XBRZ_4X, NO_VIDEO, TOTAL_VIDEO_SCALE
};

static const enum videoScaleIntegerType {SCALE_INT_REDUCE=0, SCALE_INT_INCREASE, SCALE_INT_1X, SCALE_INT_2X, SCALE_INT_3X, SCALE_INT_4X, SCALE_INT_5X, TOTAL_INT_SCALE};


static const enum aspectRatio { RATIO_CORE=0, RATIO_4_3, RATIO_3_2, RATIO_8_7, RATIO_10_9,
	RATIO_1_1, RATIO_5_4, RATIO_16_9, RATIO_16_10, TOTAL_VIDEO_RATIO
};

// Resoluciones estandar ofrecidas en el menu de video (la entrada 0 del menu es
// "Auto"; estas van a continuacion). En Xbox coinciden con la allow-list del
// backend (vid_modes) y estan capadas a <=720p por rendimiento. En Windows el
// fichero de config puede definir cualquier resolucion (el menu la anade como
// entrada extra si no esta en esta lista).
struct t_screen_res { int w, h; };
static const t_screen_res g_screenResolutions[] = {
#ifdef WIN
	{3840, 2160}, {2560, 1440}, {1920, 1080},
#endif
	{1280, 720}, {1024, 768}, {800, 600}, {640, 480}
};
#define TOTAL_SCREEN_RES (int)(sizeof(g_screenResolutions)/sizeof(g_screenResolutions[0]))

static const enum videoShaders {
	SHADER_NEAREST,         /* 0 */
	SHADER_BILINEAR,        /* 1 - Sharp-Bilinear-Simple */
	SHADER_BILINEAR_STD,    /* 2 - Bilinear clasico (hardware LINEAR) */
	SHADER_LCD_GRID,        /* 3 - LCD3x (handhelds, Gigaherz) */
	SHADER_SCANLINES,       /* 4 */
	SHADER_CRT,             /* 5 - CRT-Geom */
	SHADER_CRT_LOTTES,      /* 6 */
	SHADER_CRT_EASYMODE,    /* 7 */
	SHADER_HQ2X,            /* 8 */
	SHADER_HQ3X,            /* 9 */
	SHADER_HQ4X,            /* 10 */
	SHADER_XBR_LV2_FAST,    /* 11 */
	SHADER_XBR_HYLLIAN,     /* 12 - 5xBR v3.8a (rounded) smooth blend */
	TOTAL_SHADERS
};

static const enum ANIM_BACKGROUNDS {BG_TILES, BG_IMAGE, BG_HLSL, BG_HLSL2, BG_HLSL3, BG_NONE, BG_MAX};
static const enum syncOptions {OPT_SYNC_AUDIO = SYNC_TO_AUDIO, OPT_SYNC_VIDEO = SYNC_TO_VIDEO, OPT_SYNC_NONE = SYNC_NONE, TOTAL_VIDEO_SYNC};
static const enum SCRAP_GAMES {SCRAP_ALL = 0, SCRAP_NO_METADATA, SCRAP_NO_SCREENSHOT, SCRAP_NO_TITLE, SCRAP_NO_BOX, TOTAL_SCRAP_GAMES};
enum SCRAP_FROM{SC_SCREENCSRAPER, SC_THEGAMESDB, SC_MAX};