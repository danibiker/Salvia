#pragma once

#include <const/constant.h>

static const enum videoScale { FULLSCREEN=0, SCALE1X, SCALE2X, SCALE2X_ADV,
	SCALE_HQ2X_ALT, 
	SCALE_XBRZ_2X, SCALE_XBRZ_2X_TH, SCALE3X, SCALE3X_ADV, 
	SCALE_HQ3X_ALT, 
	SCALE_XBRZ_3X, SCALE_XBRZ_3X_TH, SCALE4X, 
	SCALE4X_ADV, SCALE_XBRZ_4X, NO_VIDEO, TOTAL_VIDEO_SCALE
};

static const enum aspectRatio { RATIO_CORE=0, RATIO_4_3, RATIO_3_2, RATIO_8_7, RATIO_10_9, 
	RATIO_1_1, RATIO_5_4, RATIO_16_9, RATIO_16_10, TOTAL_VIDEO_RATIO
};

static const enum videoShaders {
	SHADER_NEAREST,         /* 0 */
	SHADER_BILINEAR,        /* 1 - Sharp-Bilinear-Simple */
	SHADER_LCD_GRID,        /* 2 - LCD3x (handhelds, Gigaherz) */
	SHADER_SCANLINES,       /* 3 */
	SHADER_CRT,             /* 4 - CRT-Geom */
	SHADER_CRT_LOTTES,      /* 5 */
	SHADER_CRT_EASYMODE,    /* 6 */
	SHADER_HQ2X,            /* 7 */
	SHADER_HQ3X,            /* 8 */
	SHADER_HQ4X,            /* 9 */
	SHADER_XBR_LV2_FAST,    /* 10 */
	SHADER_XBR_HYLLIAN,     /* 11 - 5xBR v3.8a (rounded) smooth blend */
	TOTAL_SHADERS
};

//static const enum animBackgrounds {BG_WAVES, BG_TILES, BG_NONE, BG_MAX};
static const enum animBackgrounds {BG_TILES, BG_NONE, BG_MAX};

static float aspectRatioValues [] = {4/3.0f, 4/3.0f, 3/2.0f, 8/7.0f, 10/9.0f, 1, 5/4.0f, 16/9.0f, 16/10.0f, -1};
static const enum syncOptions {OPT_SYNC_AUDIO = SYNC_TO_AUDIO, OPT_SYNC_VIDEO = SYNC_TO_VIDEO, OPT_SYNC_NONE = SYNC_NONE, TOTAL_VIDEO_SYNC};
static const enum SCRAP_GAMES {SCRAP_ALL = 0, SCRAP_NO_METADATA, SCRAP_NO_SCREENSHOT, SCRAP_NO_TITLE, SCRAP_NO_BOX, TOTAL_SCRAP_GAMES};

enum SCRAP_FROM{SC_SCREENCSRAPER, SC_THEGAMESDB, SC_MAX};