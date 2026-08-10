#ifndef LIBRETRO_CORE_H
#define LIBRETRO_CORE_H 1

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <boolean.h>

#define UINT16 uint16_t
#define UINT32 uint32_t

//#define RENDER16B
#ifdef  RENDER16B
	extern uint16_t Retro_Screen[1024*1024];
	#define PIXEL_BYTES 1
	#define PIXEL_TYPE UINT16
	#define PITCH 2	
#else
	extern unsigned int Retro_Screen[1024*1024];
	#define PIXEL_BYTES 2
	#define PIXEL_TYPE UINT32
	#define PITCH 4	
#endif 

#define WINDOW_WIDTH 384//400
#define WINDOW_HEIGHT 288//300

#ifndef NO_LIBCO
#include "libco.h"

extern cothread_t mainThread;
extern cothread_t emuThread;
#endif

extern char Key_Sate[512];
extern char Key_Sate2[512];

extern int pauseg; 

#define NPLGN 10
#define NLIGN 5
#define NLETT 5

#define XSIDE  (CROP_WIDTH/NPLGN -1)
#define YSIDE  (CROP_HEIGHT/8 -1)

#define YBASE0 (CROP_HEIGHT - NLIGN*YSIDE -8)
#define XBASE0 0+4+2
#define XBASE3 0
#define YBASE3 YBASE0 -4

#define STAT_DECX 120
#define STAT_YSZ  20

#define RGB565(r, g, b)  (((r) << (5+16)) | ((g) << (5+8)) | (b<<5))
#define uint32 unsigned int
#define uint8 unsigned char
#endif
