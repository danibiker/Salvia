#pragma once

static const enum videoScale { FULLSCREEN=0, SCALE1X, SCALE2X, SCALE2X_ADV, SCALE_XBRZ_2X, SCALE3X, SCALE3X_ADV, SCALE_XBRZ_3X, SCALE_XBRZ_3X_TH, SCALE4X, 
	SCALE4X_ADV, SCALE_XBRZ_4X, NO_VIDEO, TOTAL_VIDEO_SCALE};

static const char* videoScaleStrings[] = { "Pantalla Completa", "Scale 1X", "Scale 2X", "Scale 2X Avanzado", "Xbrz 2X", "Scale 3X", "Scale 3X Avanzado",
	"Xbrz 3X", "Xbrz 3X Multihilo", "Scale 4X", "Scale 4X Avanzado", "Xbrz 4X", "Sin vídeo", "No implementado"};

/*
Ratios Estándar (Los más importantes)
- 4:3 (1.333f): El estándar de la televisión analógica. Es el ratio nativo de casi todas las consolas desde la NES hasta la Nintendo 64 y PlayStation 1.
- 16:9 (1.777f): El estándar de la alta definición actual. Se utiliza para el modo "Full" en la Xbox 360 o para juegos de la sexta generación (PS2, GameCube) que soportaban modo panorámico.
- 8:7 (1.142f): Muy común en Super Nintendo. Internamente, la SNES genera una imagen casi cuadrada (256x224), que luego la TV estiraba a 4:3. Muchos puristas prefieren jugar en 8:7 para ver los "píxeles perfectos". 
Ratios de Consolas Portátiles
- 10:9 (1.111f): Ratio original de la Game Boy y Game Boy Color (resolución 160x144).
- 3:2 (1.500f): Ratio nativo de la Game Boy Advance (resolución 240x160). Es el mismo ratio que las fotos de 35mm.
- 4:3 (1.333f): También usado por la Game Gear y la Sega Master System.
*/

static const enum aspectRatio { RATIO_CORE=0, RATIO_4_3, RATIO_3_2, RATIO_8_7, RATIO_10_9, RATIO_1_1, RATIO_5_4, RATIO_16_9, RATIO_16_10, RATIO_FILL_AVAILABLE, TOTAL_VIDEO_RATIO};
static float aspectRatioValues [] = {4/3.0f, 4/3.0f, 3/2.0f, 8/7.0f, 10/9.0f, 1, 5/4.0f, 16/9.0f, 16/10.0f, 0, -1};
static const char* aspectRatioStrings[] = { "Proporcionado por el núcleo", "4:3 (Nes, N64, Psx)", "3:2 (Gba)", "8:7 (Snes)", "10:9 (Gb, Gg, Ngp, Ws)", "1:1 (Alt: Gb, Gg, Ngp, Ws)", "5:4 (Pc, X68000)", "16:9",
	"16:10", "Rellenar todo", "No implementado"};

