#include "tiles_generic.h"
#include "driver.h"
#include "sn76496.h"
 
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 *AllMem;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;

static UINT8 DrvReset = 0;
static UINT8 DrvDips[1];

// Dip Switch and Input Definitions
static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin"      , BIT_DIGITAL  , DrvJoy1 + 0,	  "p1 coin"  },
	{"P1 Start"     , BIT_DIGITAL  , DrvJoy1 + 1,	  "p1 start" },

	{"P1 Up"        , BIT_DIGITAL  , DrvJoy1 + 2, 	"p1 up"    },
	{"P1 Down"      , BIT_DIGITAL  , DrvJoy1 + 3, 	"p1 down"  },
	{"P1 Left"      , BIT_DIGITAL  , DrvJoy1 + 4, 	"p1 left"  },
	{"P1 Right"     , BIT_DIGITAL  , DrvJoy1 + 5, 	"p1 right" },
	{"P1 Button 1"  , BIT_DIGITAL  , DrvJoy1 + 6,		"p1 fire 1"},

	{"P2 Coin"      , BIT_DIGITAL  , DrvJoy2 + 0,	  "p2 coin"  },
	{"P2 Start"     , BIT_DIGITAL  , DrvJoy2 + 1,	  "p2 start" },

	{"P2 Up"        , BIT_DIGITAL  , DrvJoy2 + 2, 	"p2 up"    },
	{"P2 Down"      , BIT_DIGITAL  , DrvJoy2 + 3, 	"p2 down"  },
	{"P2 Left"      , BIT_DIGITAL  , DrvJoy2 + 4, 	"p2 left"  },
	{"P2 Right"     , BIT_DIGITAL  , DrvJoy2 + 5, 	"p2 right" },
	{"P2 Button 1"  , BIT_DIGITAL  , DrvJoy2 + 6,		"p2 fire 1"},

	{"Reset"        , BIT_DIGITAL  , &DrvReset  ,		"reset"    },
	{"Dip Sw(1)"    , BIT_DIPSWITCH, DrvDips + 0  ,	  "dip"      },
	{"Dip Sw(2)"    , BIT_DIPSWITCH, DrvDips + 1  ,	  "dip"      },
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo ZxDIPList[]=
{
	// Default Values
	{0x0f, 0xff, 0xff, 0xc0, NULL},
	{0x10, 0xff, 0xff, 0x00, NULL},

	// Dip Sw(1)
	{0,		0xfe, 0,	4,	  "Bonus Life"},
	{0x0f, 0x01, 0x03, 0x00, "10000"},
	{0x0f, 0x01, 0x03, 0x01, "20000"},
	{0x0f, 0x01, 0x03, 0x02, "30000"},
	{0x0f, 0x01, 0x03, 0x03, "40000"},
};

STDDIPINFO(Zx)

// Zaxxon (set 1)
static struct BurnRomInfo ZaxxonRomDesc[] = {
	{ "zaxxon.3",		0x2000, 0x6e2b4a30, BRF_ESS | BRF_PRG },		//  0 Z80 code
	{ "zaxxon.2",		0x2000, 0x1c9ea398, BRF_ESS | BRF_PRG },		//  1
	{ "zaxxon.1",		0x1000, 0x1c123ef9, BRF_ESS | BRF_PRG },		//  2
	
	{ "zaxxon.14",		0x0800, 0x07bf8c52, BRF_GRA },					// chars
	{ "zaxxon.15",		0x0800, 0xc215edcb, BRF_GRA },

	{ "zaxxon.6",		0x2000, 0x6e07bb68, BRF_GRA },					// background tiles
	{ "zaxxon.5",		0x2000, 0x0a5bce6a, BRF_GRA },
	{ "zaxxon.4",		0x2000, 0xa5bf1465, BRF_GRA },
 
	{ "zaxxon.11",		0x2000, 0xeaf0dd4b, BRF_GRA },					// sprites
	{ "zaxxon.12",		0x2000, 0x1c5369c7, BRF_GRA },
	{ "zaxxon.13",		0x2000, 0xab4e8a9a, BRF_GRA },

	{ "zaxxon.8",		0x2000, 0x28d65063, BRF_GRA },					// background tilemaps
	{ "zaxxon.7",		0x2000, 0x6284c200, BRF_GRA },
	{ "zaxxon.10",		0x2000, 0xa95e61fd, BRF_GRA },
	{ "zaxxon.9",		0x2000, 0x7e42691f, BRF_GRA },

	{ "zaxxon.u98",		0x0100, 0x6cc6695b, BRF_GRA },					// palette
	{ "zaxxon.u72",		0x0100, 0xdeaa21f7, BRF_GRA },					// char lookup table
};

STD_ROM_PICK(Zaxxon)
STD_ROM_FN(Zaxxon)


/*
	convert the graphics into a format fba's generic tile handling can use
*/

static INT32 DrvGfxDecode()
{ 

	return 0;
}
 

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

 

	return 0;
}

/*
	DrvInit sets up the machine that we are emulating

	Here we:
		allocate the necessary ram
		load roms in to ram
		set up cpu(s)
		set up sound chips
		initialize fba's graphics routines
		reset the machine
*/

static INT32 DrvInit()
{
 

	return 0;
}

/*
	DrvExit exits the machine
*/

static INT32 DrvExit()
{
 

	return 0;
}

static INT32 DrvFrame()
{
 

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
 

	return 0;
}
struct BurnDriver BurnDrvZaxxon = {
	"zaxxon", NULL, NULL, NULL, "1982",
	"Zaxxon (set 1)\0", NULL, "Sega", "Zaxxon (set 1)",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING|BDF_ORIENTATION_VERTICAL,2,HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ZaxxonRomInfo,ZaxxonRomName, NULL, NULL,DrvInputInfo,ZxDIPInfo,
	DrvInit,DrvExit,DrvFrame,NULL,DrvScan,
	NULL,0x80,256,256,3,4
};