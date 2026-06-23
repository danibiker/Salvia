#include "tiles_generic.h"
#include "ds1302.h"
#include "se3208_intf.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvBIOSROM;
static UINT8 *DrvMainROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvSysRegs;
static UINT8 *DrvWorkRAM;
static UINT8 *DrvVidRegs;
static UINT8 *DrvFrameRAM;
static UINT8 *DrvTexRAM;
static UINT8 *DrvVrenderRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT32 IntHigh = 0;
static UINT32 DMActrl[2];
static UINT32 Timerctrl[4];
static UINT32 FlashCmd = 0;
static INT32 m_bank = 0;

static INT32 FlipCount = 0;

static UINT8 DrvJoy1[32];
static UINT8 DrvJoy2[32];
static UINT8 DrvJoy3[32];
static UINT32 DrvInputs[3];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static UINT32 bankdata[4];

static INT32 timers_delay[4] = { 0, 0, 0, 0 };
static INT32 timers_period[4] = { 0, 0, 0, 0 };
static INT32 timers_enable[4] = { 0, 0, 0, 0 };
static INT32 timers_flags[4] = { 0, 0, 0, 0 };

static struct BurnRomInfo emptyRomDesc[] = {
	{ "",                    0,          0, 0 },
};

static struct BurnInputInfo CrystalInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 18,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 22,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 3"},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 4"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 17,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 19,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 21,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 23,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 3"},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 4"},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 start"},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy2 + 16,	"p3 up"},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy2 + 18,	"p3 down"},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy2 + 20,	"p3 left"},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 22,	"p3 right"},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p3 fire 1"},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p3 fire 2"},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 4,	"p3 fire 3"},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoy2 + 6,	"p3 fire 4"},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p4 start"},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy2 + 17,	"p4 up"},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy2 + 19,	"p4 down"},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy2 + 21,	"p4 left"},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy2 + 23,	"p4 right"},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p4 fire 1"},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p4 fire 2"},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p4 fire 3"},
	{"P4 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p4 fire 4"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 6,	"service"},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 7,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Crystal)

static struct BurnDIPInfo CrystalDIPList[]=
{
	{0x29, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    2, "Pause"	},
	{0x29, 0x01, 0x01, 0x01, "Off"		},
	{0x29, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Free Play"	},
	{0x29, 0x01, 0x02, 0x02, "Off"		},
	{0x29, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"	},
	{0x29, 0x01, 0x04, 0x04, "Off"		},
	{0x29, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"	},
	{0x29, 0x01, 0x08, 0x08, "Off"		},
	{0x29, 0x01, 0x08, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"	},
	{0x29, 0x01, 0x10, 0x10, "Off"		},
	{0x29, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"	},
	{0x29, 0x01, 0x20, 0x20, "Off"		},
	{0x29, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"	},
	{0x29, 0x01, 0x40, 0x40, "Off"		},
	{0x29, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Test"		},
	{0x29, 0x01, 0x80, 0x80, "Off"		},
	{0x29, 0x01, 0x80, 0x00, "On"		},
};

STDDIPINFO(Crystal)

//------------------------------------------------------------------------------------------------------------------




static UINT32 *TexBase;
static UINT32 *FBBase;
static UINT32 *SOUNDREGS;

static const UINT16 ULawTo16[]=
{
	0x8000,0x8400,0x8800,0x8C00,0x9000,0x9400,0x9800,0x9C00,
	0xA000,0xA400,0xA800,0xAC00,0xB000,0xB400,0xB800,0xBC00,
	0x4000,0x4400,0x4800,0x4C00,0x5000,0x5400,0x5800,0x5C00,
	0x6000,0x6400,0x6800,0x6C00,0x7000,0x7400,0x7800,0x7C00,
	0xC000,0xC200,0xC400,0xC600,0xC800,0xCA00,0xCC00,0xCE00,
	0xD000,0xD200,0xD400,0xD600,0xD800,0xDA00,0xDC00,0xDE00,
	0x2000,0x2200,0x2400,0x2600,0x2800,0x2A00,0x2C00,0x2E00,
	0x3000,0x3200,0x3400,0x3600,0x3800,0x3A00,0x3C00,0x3E00,
	0xE000,0xE100,0xE200,0xE300,0xE400,0xE500,0xE600,0xE700,
	0xE800,0xE900,0xEA00,0xEB00,0xEC00,0xED00,0xEE00,0xEF00,
	0x1000,0x1100,0x1200,0x1300,0x1400,0x1500,0x1600,0x1700,
	0x1800,0x1900,0x1A00,0x1B00,0x1C00,0x1D00,0x1E00,0x1F00,
	0xF000,0xF080,0xF100,0xF180,0xF200,0xF280,0xF300,0xF380,
	0xF400,0xF480,0xF500,0xF580,0xF600,0xF680,0xF700,0xF780,
	0x0800,0x0880,0x0900,0x0980,0x0A00,0x0A80,0x0B00,0x0B80,
	0x0C00,0x0C80,0x0D00,0x0D80,0x0E00,0x0E80,0x0F00,0x0F80,
	0xF800,0xF840,0xF880,0xF8C0,0xF900,0xF940,0xF980,0xF9C0,
	0xFA00,0xFA40,0xFA80,0xFAC0,0xFB00,0xFB40,0xFB80,0xFBC0,
	0x0400,0x0440,0x0480,0x04C0,0x0500,0x0540,0x0580,0x05C0,
	0x0600,0x0640,0x0680,0x06C0,0x0700,0x0740,0x0780,0x07C0,
	0xFC00,0xFC20,0xFC40,0xFC60,0xFC80,0xFCA0,0xFCC0,0xFCE0,
	0xFD00,0xFD20,0xFD40,0xFD60,0xFD80,0xFDA0,0xFDC0,0xFDE0,
	0x0200,0x0220,0x0240,0x0260,0x0280,0x02A0,0x02C0,0x02E0,
	0x0300,0x0320,0x0340,0x0360,0x0380,0x03A0,0x03C0,0x03E0,
	0xFE00,0xFE10,0xFE20,0xFE30,0xFE40,0xFE50,0xFE60,0xFE70,
	0xFE80,0xFE90,0xFEA0,0xFEB0,0xFEC0,0xFED0,0xFEE0,0xFEF0,
	0x0100,0x0110,0x0120,0x0130,0x0140,0x0150,0x0160,0x0170,
	0x0180,0x0190,0x01A0,0x01B0,0x01C0,0x01D0,0x01E0,0x01F0,
	0x0000,0x0008,0x0010,0x0018,0x0020,0x0028,0x0030,0x0038,
	0x0040,0x0048,0x0050,0x0058,0x0060,0x0068,0x0070,0x0078,
	0xFF80,0xFF88,0xFF90,0xFF98,0xFFA0,0xFFA8,0xFFB0,0xFFB8,
	0xFFC0,0xFFC8,0xFFD0,0xFFD8,0xFFE0,0xFFE8,0xFFF0,0xFFF8,
};

#define STATUS		SOUNDREGS[0x404/4]
#define CURSADDR(chan)	(SOUNDREGS[(0x20/4)*chan+0x00])
#define DSADDR(chan)	((SOUNDREGS[(0x20/4)*chan+0x08/4]>>0)&0xffff)
#define LOOPBEGIN(chan)	(SOUNDREGS[(0x20/4)*chan+0x0c/4]&0x3fffff)
#define LOOPEND(chan)	(SOUNDREGS[(0x20/4)*chan+0x10/4]&0x3fffff)
#define ENVVOL(chan)	(SOUNDREGS[(0x20/4)*chan+0x04/4]&0xffffff)

void vrender0Init(UINT8 *texture, UINT8 *frame, UINT8 *soundram)
{
	TexBase = (UINT32*)texture;
	FBBase = (UINT32*)frame;
	SOUNDREGS = (UINT32*)soundram;
}


void vrender0_sound_write(UINT16 data)// call at offset 0x404
{
	if (data&0x8000)
	{
		UINT32 c=data&0x1f;
		STATUS|=1<<c;
		CURSADDR(c)=0;
	}
	else
	{
		STATUS&=~(1<<(data&0x1f));
	}
}

static void vrender0_update(INT16 *pbuffer, INT32 len)
{
	INT16 *SAMPLES;
	UINT32 st=STATUS;
	signed int lsample=0,rsample=0;
	UINT32 CLK=(SOUNDREGS[0x600/4]>>0)&0xff;
	 INT32 NCH=(SOUNDREGS[0x600/4]>>8)&0xff;
	UINT32 CT1=(SOUNDREGS[0x600/4]>>16)&0xff;
	UINT32 CT2=(SOUNDREGS[0x600/4]>>24)&0xff;
	int div;
	int s;

	if(CT1&0x20)
		SAMPLES=(INT16 *)TexBase;
	else
		SAMPLES=(INT16 *)FBBase;

	if(CLK)
		div=((30<<16)|0x8000)/(CLK+1);
	else
		div=1<<16;

	for(s=0;s<len;++s)
	{
		int i;
		lsample=rsample=0;
		for(i=0;i<=NCH;++i)
		{
			signed int sample;
			UINT32 cur      = CURSADDR(i);
			UINT32 a        = LOOPBEGIN(i)+(cur>>10);
			UINT8 Mode      = SOUNDREGS[(0x20/4)*i+0x8/4]>>24;
			signed int LVOL = SOUNDREGS[(0x20/4)*i+0xc/4]>>24;
			signed int RVOL = SOUNDREGS[(0x20/4)*i+0x10/4]>>24;

			INT32 DSADD=(DSADDR(i)*div)>>16;

			if(!(st&(1<<i)) || !(CT2&0x80))
				continue;

			if(Mode&0x10)		//u-law
			{
				UINT16 stn=SAMPLES[a];
				if((cur&0x200))
					stn>>=8;
				sample=(signed short)ULawTo16[stn&0xff];
			}
			else
			{
				if(Mode&0x20)	//8bit
				{
					UINT16 stn=SAMPLES[a];
					if((cur&0x200))
						stn>>=8;
					sample=(signed short) (((signed char) (stn&0xff))<<8);
				}
				else				//16bit
				{
					sample=SAMPLES[a];
				}
			}

			CURSADDR(i)+=DSADD;

			if(a>=LOOPEND(i))
			{
				if(Mode&1)	//Loop
					CURSADDR(i)=0;
				else
				{
					STATUS&=~(1<<(i&0x1f));
					break;
				}
			}

			lsample+=(sample*LVOL)>>8;
			rsample+=(sample*RVOL)>>8;
		}

		if(lsample> 32767) lsample=32767;
		if(lsample<-32768) lsample=-32768;
		if(rsample> 32767) rsample=32767;
		if(rsample<-32768) rsample=-32768;
		pbuffer[s*2+0]=lsample;
		pbuffer[s*2+1]=rsample;
	}
}

static void IntReq(int num)
{
	UINT32 IntEn = se3208_read_dword(0x01800c08);
	UINT32 IntPend = se3208_read_dword(0x01800c0c);

	if (IntEn & (1 << num))
	{
		IntPend |= (1 << num);
		se3208_write_dword(0x01800c0c, IntPend);

		se3208SetIRQLine(SE3208_INT_LINE, SE3208_IRQSTATUS_ACK);
	}
}

static void bankswitch(INT32 data)
{
	INT32 bank = (data >> 1) & 0x07;
	if (bank >= 3) bank = 3;
	m_bank = bank;

	se3208MapMemory(DrvMainROM + bank * 0x1000000, 0x05000000, 0x05ffffff, SE3208_ROM);
}

static void DMA_write(int which, UINT32 data)
{
	if (((data ^ DMActrl[which]) & (1 << 10)) && (data & (1 << 10)))	//DMAOn
	{
		UINT32 CTR = data;
		UINT32 SRC = se3208_read_dword(0x01800804 + which * 0x10);
		UINT32 DST = se3208_read_dword(0x01800808 + which * 0x10);
		UINT32 CNT = se3208_read_dword(0x0180080C + which * 0x10);

		if (CTR & 0x2)	//32 bits
		{
			for (UINT32 i = 0; i < CNT; ++i)
			{
				UINT32 v = se3208_read_dword(SRC + i * 4);
				se3208_write_dword(DST + i * 4, v);
			}
		}
		else if (CTR & 0x1)	//16 bits
		{
			for (UINT32 i = 0; i < CNT; ++i)
			{
				UINT16 v = se3208_read_word(SRC + i * 2);
				se3208_write_word(DST + i * 2, v);
			}
		}
		else	//8 bits
		{
			for (UINT32 i = 0; i < CNT; ++i)
			{
				UINT8 v = se3208_read_byte(SRC + i);
				se3208_write_byte(DST + i, v);
			}
		}

		data &= ~(1 << 10);
		se3208_write_dword(0x0180080C + which * 0x10, 0);
		IntReq(7 + which);
	}

	DMActrl[which] = data;
}

static void PIO_write(UINT32 data)
{
	UINT32 RST = data & 0x01000000;
	UINT32 CLK = data & 0x02000000;
	UINT32 DAT = data & 0x10000000;

	if (!RST) ds1302_reset();

	ds1302_dat_write(DAT ? 1 : 0);
	ds1302_clk_write(CLK ? 1 : 0);

	if (ds1302_read())
		se3208_write_dword(0x01802008, se3208_read_dword(0x01802008) | 0x10000000);
	else
		se3208_write_dword(0x01802008, se3208_read_dword(0x01802008) & (~0x10000000));
}

static void FlashCmd_write()
{
	if ((FlashCmd & 0xff) == 0xff)
	{
		if (m_bank <= 2) {
			*((UINT32 *)(DrvMainROM + m_bank * 0x1000000)) = bankdata[m_bank];
		}
	}

	if ((FlashCmd & 0xff) == 0x90)
	{
		if (m_bank <= 2) {
			*((UINT32 *)(DrvMainROM + m_bank * 0x1000000)) = 0x00180089; // Intel 128MBit
		}
	}
}	

static void timer_callback(INT32 which)
{
	static const INT32 num[] = { 0, 1, 9, 10 };

	UINT32 ctrl = se3208_read_dword(0x1801400 + which * 8);

	if (~ctrl & 2) {
		se3208_write_dword(0x1801400 + which * 8, ctrl & ~1);
		Timerctrl[which] &= ~1;
	}

	IntReq(num[which]);
}

static void Timer_write(int which, UINT32 data)
{
	if (((data ^ Timerctrl[which]) & 1) && (data & 1))
	{
		int PD = (data >> 8) & 0xff;
		UINT32 TCV = se3208_read_dword(0x01801404 + which * 8);

		float period = 43000000.00000000 * (((PD + 1.00000000) * (TCV + 1.00000000)) * (1.000000000 / 43000000.00000000));

		bprintf (0, _T("TIMER: %d FLAG: %d, CYCLES: %d\n"), which, Timerctrl[which] & 2, (INT32)period);

		timers_enable[which] = 1;
		timers_period[which] = (INT32)period;

		timers_delay[which] = se3208TotalCycles() + timers_period[which];
		timers_flags[which] = Timerctrl[which] & 2; // 2 is periodic, 0 is oneshot
	}

	Timerctrl[which] = data;
}

static void IntAck_write(UINT32 data, UINT32 mem_mask)
{
	UINT32 IntPend = se3208_read_dword(0x01800c0c);

	if (mem_mask & 0x00ff)
	{
		IntPend &= ~(1 << (data & 0x1f));
		se3208_write_dword(0x01800c0c, IntPend);

		if (!IntPend) {

			bprintf (0, _T("INTERRUPT clear!\n"));

			se3208SetIRQLine(SE3208_INT_LINE, SE3208_IRQSTATUS_NONE);
		}
	}

	if (mem_mask & 0xff00)
		IntHigh = (data >> 8) & 7;
}

#define DLOG

void crystalsys_write_byte(UINT32 a, UINT8 d)
{
#ifdef DLOG
	bprintf (0, _T("%8.8x, WB %8.8x, %2.2x\n"), se3208GetPC(-1), a, d);
#endif
	if ((a & 0xfffff000) == 0x04800000) {
		if ((a & 0xffc) == 0x404) {
			vrender0_sound_write(d);
		} else {
			*((UINT8*)(DrvVrenderRAM + (a & 0xfff))) = d;
		}
		return;
	}

	if ((a & 0xffff0000) == 0x01800000) {
		*((UINT8*)(DrvSysRegs + (a & 0xffff))) = d;
	}
}

void crystalsys_write_word(UINT32 a, UINT16 d)
{
	// most writes are to 
	if ((a & 0xfffff000) == 0x04800000) {
		if ((a & 0xffc) == 0x404) {
			vrender0_sound_write(d);
		} else {
			*((UINT16*)(DrvVrenderRAM + (a & 0xfff))) = d;
		}
		return;
	}

#ifdef DLOG
	bprintf (0, _T("%8.8x WW %8.8x, %4.4x\n"), se3208GetPC(-1), a, d);
#endif

	if ((a & 0xffff0000) == 0x01800000) {
		*((UINT16*)(DrvSysRegs + (a & 0xfffe))) = d;
	}

	switch (a)
	{
		case 0x01280000:
		case 0x01280002:
			bankswitch(d);
		return;

		case 0x01800c04:
		case 0x01800c06:
			IntAck_write(d * (a & 2) * 8, 0xffff << ((a & 2) * 8));
		return;

		case 0x01801400:
		case 0x01801402:
			Timer_write(0, d);
		return;

		case 0x01801408:
		case 0x0180140a:
			Timer_write(1, d);
		return;

		case 0x01801410:
		case 0x01801412:
			Timer_write(2, d);
		return;

		case 0x01801418:
		case 0x0180141a:
			Timer_write(3, d);
		return;

		case 0x01802004:
		case 0x01802006:
			PIO_write(d);
		return;

		case 0x01800800:
		case 0x01800802:
			DMA_write(0, d);
		return;

		case 0x01800810:
		case 0x01800812:
			DMA_write(1, d);
		return;

		case 0x030000a4:
		case 0x030000a6:
			// flipcount_w
		return;

		case 0x05000000:
		case 0x05000002:
			FlashCmd = d;
			FlashCmd_write();
		return;
	}
}

void crystalsys_write_long(UINT32 a, UINT32 d)
{
#ifdef DLOG
	bprintf (0, _T("%8.8x WL %8.8x, %8.8x\n"), se3208GetPC(-1), a, d);
#endif

	if ((a & 0xfffff000) == 0x04800000) {
		if ((a & 0xffc) == 0x404) {
			vrender0_sound_write(d);
		} else {
			*((UINT32*)(DrvVrenderRAM + (a & 0xfff))) = d;
		}
		return;
	}

	if ((a & 0xffff0000) == 0x01800000) {
		*((UINT32*)(DrvSysRegs + (a & 0xfffc))) = d;
	}

	switch (a)
	{
		case 0x01800c04:
			IntAck_write(d, 0xffffffff);
		return;

		case 0x01280000:
			bankswitch(d);
		return;

		case 0x01801400:
			Timer_write(0, d);
		return;

		case 0x01801408:
			Timer_write(1, d);
		return;

		case 0x01801410:
			Timer_write(2, d);
		return;

		case 0x01801418:
			Timer_write(3, d);
		return;

		case 0x01802004:
			PIO_write(d);
		return;

		case 0x01800800:
			DMA_write(0, d);
		return;

		case 0x01800810:
			DMA_write(1, d);
		return;

		case 0x030000a4:
			// flipcount_w
		return;

		case 0x05000000:
			FlashCmd = d;
			FlashCmd_write();
		return;
	}
}

UINT8 crystalsys_read_byte(UINT32 a)
{
#ifdef DLOG
	bprintf (0, _T("%8.8x, RB %8.8x\n"), se3208GetPC(-1), a);
#endif

	switch (a)
	{
		case 0x01200000:
		case 0x01200001:
		case 0x01200002:
		case 0x01200003:
		case 0x01200004:
		case 0x01200005:
		case 0x01200006:
		case 0x01200007:
		case 0x01200008:
		case 0x01200009:
		case 0x0120000a:
		case 0x0120000b:			// input_r
			return ~0;

		case 0x0120000c:
		case 0x0120000d:
		case 0x0120000e:
		case 0x0120000f:
			return 0;

		case 0x030000a4:
		case 0x030000a5:
		case 0x030000a6:
		case 0x030000a7:			// flipcount_r
			return 0;
	}

	return 0;
}

UINT16 crystalsys_read_word(UINT32 a)
{
#ifdef DLOG
	bprintf (0, _T("%8.8x RW %8.8x\n"), se3208GetPC(-1), a);
#endif
	switch (a)
	{
		case 0x01200000: return DrvInputs[0] >> 16;
		case 0x01200002: return DrvInputs[0];
		case 0x01200004: return DrvInputs[1] >> 16;
		case 0x01200006: return DrvInputs[1];	 // right?
		case 0x01200008: return DrvInputs[2];
		case 0x0120000a: return DrvDips[0];

		case 0x030000a4:
		case 0x030000a6:			// flipcount_r
			return 0;
	}

	return 0;
}

UINT32 crystalsys_read_long(UINT32 a)
{
#ifdef DLOG
	bprintf (0, _T("%8.8x RL %8.8x\n"), se3208GetPC(-1), a);
#endif

	switch (a)
	{
		case 0x01200000: return DrvInputs[0];
		case 0x01200004: return DrvInputs[1];

		case 0x01200008: {
			UINT32 ret = ((DrvInputs[2] & 0xff) << 16) | (DrvDips[0]);
			return ret;
		}

		case 0x030000a4:			// flipcount_r
			return 0;
	}

	return 0;
}

static INT32 crystalsys_irqcallback(INT32)
{
	UINT32 IntPend = se3208_read_dword(0x01800c0c);
	int i;

	for (i = 0; i < 32; ++i)
	{
		if ((IntPend >> i) & 1)
		{
			return (IntHigh << 5) | i;
		}
	}

	return 0;
}

void crystalPalforce();

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	se3208Open(0);
	bankswitch(0);
	se3208Reset();
	se3208Close();

	crystalPalforce();
	// audio reset

	FlashCmd = 0xff;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvBIOSROM	= Next; Next += 0x020000;
	DrvMainROM	= Next; Next += 0x4000000;

	DrvPalette	= (UINT32*)Next; Next += 0x10000 * sizeof(UINT32);

	DrvNVRAM	= Next; Next += 0x010000;

	AllRam		= Next;

	DrvSysRegs	= Next; Next += 0x010000;
	DrvWorkRAM	= Next; Next += 0x800000;
	DrvVidRegs	= Next; Next += 0x010000;
	DrvTexRAM	= Next; Next += 0x800000;
	DrvFrameRAM	= Next; Next += 0x800000;
	DrvVrenderRAM	= Next; Next += 0x001000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static inline void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x10000; i++)
	{
		INT32 r = (i >> 11) & 0x1f;
		INT32 g = (i >>  5) & 0x3f;
		INT32 b = (i >>  0) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 2) | (g >> 4);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static INT32 DrvInit(INT32 romcount, void (*pLoadCallback)())
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvBIOSROM, 0x80, 1)) return 1; // bios

		memset (DrvMainROM, 0xff, 0x4000000);

		memset (bankdata, 0xff, 4 * sizeof(UINT32));

		for (INT32 i = 0; i < romcount; i++) {
			if (BurnLoadRom(DrvMainROM + i * 0x1000000, i, 1)) return 1;

			bankdata[i] = *((UINT32*)(DrvMainROM + i * 0x1000000));
		}

		if (pLoadCallback) {
			pLoadCallback();
		}

		DrvPaletteInit();
	}

	se3208Init(0);
	se3208Open(0);
	se3208MapMemory(DrvBIOSROM,		0x00000000, 0x0001ffff, SE3208_ROM);
	se3208MapMemory(DrvNVRAM,		0x01400000, 0x0140ffff, SE3208_RAM);
	se3208MapMemory(DrvSysRegs,		0x01800000, 0x01802fff, SE3208_ROM);
	se3208MapMemory(DrvSysRegs + 0x3000,	0x01803000, 0x0180ffff, SE3208_RAM);
	se3208MapMemory(DrvWorkRAM,		0x02000000, 0x027fffff, SE3208_RAM);
	se3208MapMemory(DrvVidRegs,		0x03000000, 0x0300ffff, SE3208_RAM);
	se3208MapMemory(DrvTexRAM,		0x03800000, 0x03ffffff, SE3208_RAM);
	se3208MapMemory(DrvFrameRAM,		0x04000000, 0x047fffff, SE3208_RAM);
	se3208MapMemory(DrvVrenderRAM,		0x04800000, 0x04800fff, SE3208_ROM);
//	se3208MapMemory(DrvMainROM,		0x05000000, 0x05ffffff, SE3208_ROM);
	se3208SetWriteByteHandler(crystalsys_write_byte);
	se3208SetWriteWordHandler(crystalsys_write_word);
	se3208SetWriteLongHandler(crystalsys_write_long);
	se3208SetReadByteHandler(crystalsys_read_byte);
	se3208SetReadWordHandler(crystalsys_read_word);
	se3208SetReadLongHandler(crystalsys_read_long);
	se3208SetIRQCallback(crystalsys_irqcallback);
	se3208Close();

	vrender0Init(DrvTexRAM, DrvFrameRAM, DrvVrenderRAM);
	// video init

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	// video exit
	// sound exit

	se3208Exit();

	BurnFree (AllMem);

	return 0;
}



typedef struct
{
	UINT16 *Dest;
	UINT32 Pitch;	//in UINT16s
	UINT32 w,h;
	UINT32 Tx;
	UINT32 Ty;
	UINT32 Txdx;
	UINT32 Tydx;
	UINT32 Txdy;
	UINT32 Tydy;
	UINT16 TWidth;
	UINT16 THeight;
	union _u
	{
		UINT8 *Imageb;
		UINT16 *Imagew;
	} u;
	UINT16 *Tile;
	UINT16 *Pal;
	UINT32 TransColor;
	UINT32 Shade;
	UINT8 Clamp;
	UINT8 Trans;
	UINT8 SrcAlpha;
	UINT32 SrcColor;
	UINT8 DstAlpha;
	UINT32 DstColor;
} _Quad;

typedef struct
{
	UINT32 Tx;
	UINT32 Ty;
	UINT32 Txdx;
	UINT32 Tydx;
	UINT32 Txdy;
	UINT32 Tydy;
	UINT32 SrcAlphaColor;
	UINT32 SrcBlend;
	UINT32 DstAlphaColor;
	UINT32 DstBlend;
	UINT32 ShadeColor;
	UINT32 TransColor;
	UINT32 TileOffset;
	UINT32 FontOffset;
	UINT32 PalOffset;
	UINT32 PaletteBank;
	UINT32 TextureMode;
	UINT32 PixelFormat;
	UINT32 Width;
	UINT32 Height;
} _RenderState;

typedef struct _vr0video_state  vr0video_state;
struct _vr0video_state
{
	UINT16 InternalPalette[256];
	UINT32 LastPalUpdate;

	_RenderState RenderState;
};

static vr0video_state vrender0_video;

#define NOTRANSCOLOR	0xecda

#define RGB32(r,g,b) ((r << 16) | (g << 8) | (b << 0))
#define RGB16(r,g,b) ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | ((b & 0xf8) >> 3)

INLINE UINT16 RGB32TO16(UINT32 rgb)
{
	return (((rgb >> (16 + 3)) & 0x1f) << 11) | (((rgb >> (8 + 2)) & 0x3f) << 5) | (((rgb >> (3)) & 0x1f) << 0);
}

#define EXTRACTR8(Src)	(((Src >> 11) << 3) & 0xff)
#define EXTRACTG8(Src)	(((Src >>  5) << 2) & 0xff)
#define EXTRACTB8(Src)	(((Src >>  0) << 3) & 0xff)

INLINE UINT16 Shade(UINT16 Src, UINT32 Shade)
{
	UINT32 scr = (EXTRACTR8(Src) * ((Shade >> 16) & 0xff)) >> 8;
	UINT32 scg = (EXTRACTG8(Src) * ((Shade >>  8) & 0xff)) >> 8;
	UINT32 scb = (EXTRACTB8(Src) * ((Shade >>  0) & 0xff)) >> 8;
	return RGB16(scr, scg, scb);
}

static UINT16 Alpha(_Quad *Quad, UINT16 Src, UINT16 Dst)
{
	UINT32 scr = (EXTRACTR8(Src) * ((Quad->Shade >> 16) & 0xff)) >> 8;
	UINT32 scg = (EXTRACTG8(Src) * ((Quad->Shade >>  8) & 0xff)) >> 8;
	UINT32 scb = (EXTRACTB8(Src) * ((Quad->Shade >>  0) & 0xff)) >> 8;
	UINT32 dcr = EXTRACTR8(Dst);
	UINT32 dcg = EXTRACTG8(Dst);
	UINT32 dcb = EXTRACTB8(Dst);

	UINT32 smulr, smulg, smulb;
	UINT32 dmulr, dmulg, dmulb;

	switch (Quad->SrcAlpha & 0x1f)
	{
		case 0x01:
			smulr = smulg = smulb = 0;
			break;
		case 0x02:
			smulr = (Quad->SrcColor >> 16) & 0xff;
			smulg = (Quad->SrcColor >>  8) & 0xff;
			smulb = (Quad->SrcColor >>  0) & 0xff;
			break;
		case 0x04:
			smulr = scr;
			smulg = scg;
			smulb = scb;
			break;
		case 0x08:
			smulr = (Quad->DstColor >> 16) & 0xff;
			smulg = (Quad->DstColor >>  8) & 0xff;
			smulb = (Quad->DstColor >>  0) & 0xff;
			break;
		case 0x10:
			smulr = dcr;
			smulg = dcg;
			smulb = dcb;
			break;
		default:
			smulr = smulg = smulb = 0;
			break;
	}

	if (Quad->SrcAlpha & 0x20)
	{
		smulr = 0x100 - smulr;
		smulg = 0x100 - smulg;
		smulb = 0x100 - smulb;
	}

	switch (Quad->DstAlpha & 0x1f)
	{
		case 0x01:
			dmulr = dmulg = dmulb = 0;
			break;
		case 0x02:
			dmulr = (Quad->SrcColor >> 16) & 0xff;
			dmulg = (Quad->SrcColor >>  8) & 0xff;
			dmulb = (Quad->SrcColor >>  0) & 0xff;
			break;
		case 0x04:
			dmulr = scr;
			dmulg = scg;
			dmulb = scb;
			break;
		case 0x08:
			dmulr = (Quad->DstColor >> 16) & 0xff;
			dmulg = (Quad->DstColor >>  8) & 0xff;
			dmulb = (Quad->DstColor >>  0) & 0xff;
			break;
		case 0x10:
			dmulr = dcr;
			dmulg = dcg;
			dmulb = dcb;
			break;
		default:
			dmulr = dmulg = dmulb = 0;
			break;
	}

	if (Quad->DstAlpha&0x20)
	{
		dmulr = 0x100 - dmulr;
		dmulg = 0x100 - dmulg;
		dmulb = 0x100 - dmulb;
	}


	dcr = (scr * smulr + dcr * dmulr) >> 8;
	if (dcr > 0xff)
		dcr = 0xff;

	dcg = (scg * smulg + dcg * dmulg) >> 8;
	if (dcg > 0xff)
		dcg = 0xff;

	dcb = (scb * smulb + dcb * dmulb) >> 8;
	if (dcb > 0xff)
		dcb = 0xff;

	return RGB16(dcr, dcg, dcb);
}

#define TILENAME(bpp, t, a) \
static void DrawQuad##bpp##t##a(_Quad *Quad)

//TRUST ON THE COMPILER OPTIMIZATIONS
#define TILETEMPL(bpp, t, a) \
TILENAME(bpp, t, a)\
{\
	UINT32 TransColor = Quad->Trans ? RGB32TO16(Quad->TransColor) : NOTRANSCOLOR;\
	UINT32 x, y;\
	UINT16 *line = Quad->Dest;\
	UINT32 y_tx = Quad->Tx, y_ty = Quad->Ty;\
	UINT32 x_tx, x_ty;\
	UINT32 Maskw = Quad->TWidth - 1;\
	UINT32 Maskh = Quad->THeight - 1;\
	UINT32 W = Quad->TWidth >> 3;\
\
	for (y = 0; y < Quad->h; ++y)\
	{\
		UINT16 *pixel = line;\
		x_tx = y_tx;\
		x_ty = y_ty;\
		for (x = 0; x < Quad->w; ++x)\
		{\
			UINT32 Offset;\
			UINT32 tx = x_tx >> 9;\
			UINT32 ty = x_ty >> 9;\
			UINT16 Color;\
			if (Quad->Clamp)\
			{\
				if (tx > Maskw)\
					goto Clamped;\
				if (ty > Maskh)\
					goto Clamped;\
			}\
			else\
			{\
				tx &= Maskw;\
				ty &= Maskh;\
			}\
\
			if (t)\
			{\
				UINT32 Index = Quad->Tile[(ty >> 3) * (W) + (tx >> 3)];\
				Offset = (Index << 6) + ((ty & 7) << 3) + (tx & 7);\
			}\
			else\
				Offset = ty * (Quad->TWidth) + tx;\
\
			if (bpp == 4)\
			{\
				UINT8 Texel = Quad->u.Imageb[Offset / 2];\
				if (Offset & 1)\
					Texel &= 0xf;\
				else\
					Texel = (Texel >> 4) & 0xf;\
				Color = Quad->Pal[Texel];\
			}\
			else if (bpp == 8)\
			{\
				UINT8 Texel = Quad->u.Imageb[Offset];\
				Texel = Quad->u.Imageb[Offset];\
				Color = Quad->Pal[Texel];\
			}\
			else if (bpp == 16)\
			{\
				Color = Quad->u.Imagew[Offset];\
			}\
			if (Color != TransColor)\
			{\
				if (a == 1)\
					*pixel = Alpha(Quad, Color, *pixel);\
				else if (a == 2)\
					*pixel = Shade(Color, Quad->Shade);\
				else\
					*pixel = Color;\
			}\
			++pixel;\
Clamped:\
			x_tx += Quad->Txdx;\
			x_ty += Quad->Tydx;\
		}\
		line += Quad->Pitch;\
		y_tx += Quad->Txdy;\
		y_ty += Quad->Tydy;\
	}\
}\

TILETEMPL(16,0,0) TILETEMPL(16,0,1) TILETEMPL(16,0,2)
TILETEMPL(16,1,0) TILETEMPL(16,1,1) TILETEMPL(16,1,2)

TILETEMPL(8,0,0) TILETEMPL(8,0,1) TILETEMPL(8,0,2)
TILETEMPL(8,1,0) TILETEMPL(8,1,1) TILETEMPL(8,1,2)

TILETEMPL(4,0,0) TILETEMPL(4,0,1) TILETEMPL(4,0,2)
TILETEMPL(4,1,0) TILETEMPL(4,1,1) TILETEMPL(4,1,2)

#undef TILENAME
#define TILENAME(bpp, t, a) \
DrawQuad##bpp##t##a

static void DrawQuadFill(_Quad *Quad)
{
	UINT32 x, y;
	UINT16 *line = Quad->Dest;
	UINT16 ShadeColor = RGB32TO16(Quad->Shade);
	for (y = 0; y < Quad->h; ++y)
	{
		UINT16 *pixel = line;
		for (x = 0; x < Quad->w; ++x)
		{
			if (Quad->SrcAlpha)
				*pixel = Alpha(Quad, Quad->Shade, *pixel);
			else
				*pixel = ShadeColor;
			++pixel;
		}
		line += Quad->Pitch;
	}
}

typedef void (*_DrawTemplate)(_Quad *);

static const _DrawTemplate DrawImage[]=
{
	TILENAME(4,0,0),
	TILENAME(8,0,0),
	TILENAME(16,0,0),
	TILENAME(16,0,0),

	TILENAME(4,0,1),
	TILENAME(8,0,1),
	TILENAME(16,0,1),
	TILENAME(16,0,1),

	TILENAME(4,0,2),
	TILENAME(8,0,2),
	TILENAME(16,0,2),
	TILENAME(16,0,2),
};

static const _DrawTemplate DrawTile[]=
{
	TILENAME(4,1,0),
	TILENAME(8,1,0),
	TILENAME(16,1,0),
	TILENAME(16,1,0),

	TILENAME(4,1,1),
	TILENAME(8,1,1),
	TILENAME(16,1,1),
	TILENAME(16,1,1),

	TILENAME(4,1,2),
	TILENAME(8,1,2),
	TILENAME(16,1,2),
	TILENAME(16,1,2),
};

#define Packet(i) se3208_read_word(PacketPtr + 2 * i)

int vrender0_ProcessPacket(UINT32 PacketPtr, UINT16 *Dest, UINT8 *TEXTURE)
{
	vr0video_state *vr0 = &vrender0_video;

	UINT32 Dx = Packet(1) & 0x3ff;
	UINT32 Dy = Packet(2) & 0x1ff;
	UINT32 Endx = Packet(3) & 0x3ff;
	UINT32 Endy = Packet(4) & 0x1ff;
	UINT32 Mode = 0;
	UINT16 Packet0 = Packet(0);

	if (Packet0 & 0x81)	//Sync or ASync flip
	{
		vr0->LastPalUpdate = 0xffffffff;	//Force update palette next frame
		return 1;
	}

	if (Packet0 & 0x200)
	{
		vr0->RenderState.Tx = Packet(5) | ((Packet(6) & 0x1f) << 16);
		vr0->RenderState.Ty = Packet(7) | ((Packet(8) & 0x1f) << 16);
	}
	else
	{
		vr0->RenderState.Tx = 0;
		vr0->RenderState.Ty = 0;
	}
	if (Packet0 & 0x400)
	{
		vr0->RenderState.Txdx = Packet(9)  | ((Packet(10) & 0x1f) << 16);
		vr0->RenderState.Tydx = Packet(11) | ((Packet(12) & 0x1f) << 16);
		vr0->RenderState.Txdy = Packet(13) | ((Packet(14) & 0x1f) << 16);
		vr0->RenderState.Tydy = Packet(15) | ((Packet(16) & 0x1f) << 16);
	}
	else
	{
		vr0->RenderState.Txdx = 1 << 9;
		vr0->RenderState.Tydx = 0;
		vr0->RenderState.Txdy = 0;
		vr0->RenderState.Tydy = 1 << 9;
	}
	if (Packet0 & 0x800)
	{
		vr0->RenderState.SrcAlphaColor = Packet(17) | ((Packet(18) & 0xff) << 16);
		vr0->RenderState.SrcBlend = (Packet(18) >> 8) & 0x3f;
		vr0->RenderState.DstAlphaColor = Packet(19) | ((Packet(20) & 0xff) << 16);
		vr0->RenderState.DstBlend = (Packet(20) >> 8) & 0x3f;
	}
	if (Packet0 & 0x1000)
		vr0->RenderState.ShadeColor = Packet(21) | ((Packet(22) & 0xff) << 16);
	if (Packet0 & 0x2000)
		vr0->RenderState.TransColor = Packet(23) | ((Packet(24) & 0xff) << 16);
	if (Packet0 & 0x4000)
	{
		vr0->RenderState.TileOffset = Packet(25);
		vr0->RenderState.FontOffset = Packet(26);
		vr0->RenderState.PalOffset = Packet(27) >> 3;
		vr0->RenderState.PaletteBank = (Packet(28) >> 8) & 0xf;
		vr0->RenderState.TextureMode = Packet(28) & 0x1000;
		vr0->RenderState.PixelFormat = (Packet(28) >> 6) & 3;
		vr0->RenderState.Width  = 8 << ((Packet(28) >> 0) & 0x7);
		vr0->RenderState.Height = 8 << ((Packet(28) >> 3) & 0x7);
	}

	if (Packet0 & 0x40 && vr0->RenderState.PalOffset != vr0->LastPalUpdate)
	{
		UINT32 *Pal = (UINT32*) (TEXTURE + 1024 * vr0->RenderState.PalOffset);
		UINT16 Trans = RGB32TO16(vr0->RenderState.TransColor);
		int i;
		for (i = 0; i < 256; ++i)
		{
			UINT32 p = Pal[i];
			UINT16 v = RGB32TO16(p);
			if ((v == Trans && p != vr0->RenderState.TransColor) || v == NOTRANSCOLOR)	//Error due to conversion. caused transparent
			{
				if ((v & 0x1f) != 0x1f)
					v++;									//Make the color a bit different (blueish) so it's not
				else
					v--;
			}
			vr0->InternalPalette[i] = v;						//made transparent by mistake
		}
		vr0->LastPalUpdate = vr0->RenderState.PalOffset;
	}

	if (Packet0 & 0x100)
	{
		_Quad Quad;

		Quad.Pitch = 512;

		if (Packet0 & 2)
		{
			Quad.SrcAlpha = vr0->RenderState.SrcBlend;
			Quad.DstAlpha = vr0->RenderState.DstBlend;
			Quad.SrcColor = vr0->RenderState.SrcAlphaColor;
			Quad.DstColor = vr0->RenderState.DstAlphaColor;
			Mode = 1;
		}
		else
			Quad.SrcAlpha = 0;

		Quad.w = 1 + Endx - Dx;
		Quad.h = 1 + Endy - Dy;

		Quad.Dest = (UINT16*) Dest;
		Quad.Dest = Quad.Dest + Dx + (Dy * Quad.Pitch);

		Quad.Tx = vr0->RenderState.Tx;
		Quad.Ty = vr0->RenderState.Ty;
		Quad.Txdx = vr0->RenderState.Txdx;
		Quad.Tydx = vr0->RenderState.Tydx;
		Quad.Txdy = vr0->RenderState.Txdy;
		Quad.Tydy = vr0->RenderState.Tydy;
		if (Packet0 & 0x10)
		{
			Quad.Shade = vr0->RenderState.ShadeColor;
			if (!Mode)		//Alpha includes Shade
				Mode = 2;
		}
		else
			Quad.Shade = RGB32(255,255,255);
		Quad.TransColor = vr0->RenderState.TransColor;
		Quad.TWidth = vr0->RenderState.Width;
		Quad.THeight = vr0->RenderState.Height;
		Quad.Trans = Packet0 & 4;
		Quad.Clamp = Packet0 & 0x20;

		if (Packet0 & 0x8)	//Texture Enable
		{
			Quad.u.Imageb = TEXTURE + 128 * vr0->RenderState.FontOffset;
			Quad.Tile = (UINT16*) (TEXTURE + 128 * vr0->RenderState.TileOffset);
			if (!vr0->RenderState.PixelFormat)
				Quad.Pal = vr0->InternalPalette + (vr0->RenderState.PaletteBank * 16);
			else
				Quad.Pal = vr0->InternalPalette;
			if (vr0->RenderState.TextureMode)	//Tiled
				DrawTile[vr0->RenderState.PixelFormat + 4 * Mode](&Quad);
			else
				DrawImage[vr0->RenderState.PixelFormat + 4 * Mode](&Quad);
		}
		else
			DrawQuadFill(&Quad);
	}
	return 0;
}


static UINT16 GetVidReg( UINT16 reg )
{
	return se3208_read_word(0x03000000 + reg);
}

static void SetVidReg( UINT16 reg, UINT16 val )
{
	se3208_write_word(0x03000000 + reg, val);
}

void crystalPalforce()
{
	vr0video_state *vr0 = &vrender0_video;
	memset(vr0->InternalPalette, 0, sizeof(vr0->InternalPalette));
	vr0->LastPalUpdate = 0xffffffff;
}


static void crystalDraw()
{
	int DoFlip;

	UINT32 B0 = 0x0;
	UINT32 B1 = (GetVidReg(0x90) & 0x8000) ? 0x400000 : 0x100000;
	UINT16 *Front, *Back;
	UINT16 *Visible, *DrawDest;
	UINT16 *srcline;
	int y;
	UINT16 head, tail;
	UINT32 width = nScreenWidth;

	if (GetVidReg(0x8e) & 1)
	{
		Front = (UINT16*) (DrvFrameRAM + B1);
		Back  = (UINT16*) (DrvFrameRAM + B0);
	}
	else
	{
		Front = (UINT16*) (DrvFrameRAM + B0);
		Back  = (UINT16*) (DrvFrameRAM + B1);
	}

	Visible  = (UINT16*) Front;
	DrawDest = (UINT16 *) DrvFrameRAM;

	if (GetVidReg(0x8c) & 0x80)
		DrawDest = Front;
	else
		DrawDest = Back;

	srcline = (UINT16 *) DrawDest;

	DoFlip = 0;
	head = GetVidReg(0x82);
	tail = GetVidReg(0x80);
	while ((head & 0x7ff) != (tail & 0x7ff))
	{
		DoFlip = vrender0_ProcessPacket(0x03800000 + head * 64, DrawDest, DrvTexRAM);
		head++;
		head &= 0x7ff;
		if (DoFlip)
			break;
	}

	if (DoFlip)
		SetVidReg(0x8e, GetVidReg(0x8e) ^ 1);

	srcline = (UINT16 *) Visible;

	for (y = 0; y < 240; y++)
		memcpy (pTransDraw + y * nScreenWidth, &srcline[y * 512], width * 2);
}

static void crystalEOF()
{
	UINT16 head, tail;
	int DoFlip = 0;

	head = GetVidReg(0x82);
	tail = GetVidReg(0x80);
	while ((head & 0x7ff) != (tail & 0x7ff))
	{
		UINT16 Packet0 = se3208_read_word(0x03800000 + head * 64);
		if (Packet0 & 0x81)
			DoFlip = 1;
		head++;
		head &= 0x7ff;
		if (DoFlip) break;
	}

	SetVidReg(0x82, head);

	if (DoFlip)
	{
		if (FlipCount)
			FlipCount--;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	se3208NewFrame(); // timers

	{
		memset (DrvInputs, 0xff, 3 * sizeof(UINT32));
		for (INT32 i = 0; i < 32; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nCyclesTotal = 43000000 / 60;
	INT32 nInterleave = nCyclesTotal; //10000;
	INT32 nCyclesDone  = 0;
	INT32 nCyclesDonePrev = 0;

	se3208Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal / nInterleave;

		nCyclesDone += se3208Run(nSegment);

		for (INT32 j = 0; j < 4; j++)
		{
			if (timers_enable[j])
			{
				if (timers_delay[j] >= nCyclesDonePrev && timers_delay[j] <= nCyclesDone)
				{
					timer_callback(j);

					timers_delay[j] += timers_period[j];

					if (timers_flags[j] == 0) { // oneshot
						timers_enable[j] = 0;	// disable
					}
					bprintf (0, _T("Timer triggered: %d\n"), j);
				}
			}
		}

		nCyclesDonePrev = nCyclesDone;

		if (i == (nInterleave - 1)) IntReq(24);
	}

	crystalDraw();

	for (INT32 j = 0; j < 4; j++) {
		if (timers_enable[j]) {
			timers_delay[j] -= nCyclesDone;
		}
	}

	if (pBurnSoundOut) {
		vrender0_update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	crystalEOF();

	se3208Close();

	return 0;
}




// Crystal System BIOS

static struct BurnRomInfo crysbiosRomDesc[] = {
	{ "mx27l1000.u14",	0x020000, 0xBEFF39A9, BRF_PRG | BRF_BIOS },	// Bios
};

STD_ROM_PICK(crysbios)
STD_ROM_FN(crysbios)

static INT32 crysbiosInit()
{
	return DrvInit(0, NULL);
}

struct BurnDriver BurnDrvCrysbios = {
	"crysbios", NULL, NULL, NULL, "2001",
	"Crystal System BIOS\0", "BIOS only", "BrezzaSoft", "Crystal System",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_MISC_POST90S, GBF_BIOS, 0,
	NULL, crysbiosRomInfo, crysbiosRomName, NULL, NULL, CrystalInputInfo, CrystalDIPInfo,
	crysbiosInit, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// The Crystal of Kings

static struct BurnRomInfo cryskingRomDesc[] = {
	{ "bcsv0004f01.u1",		0x1000000, 0x8feff120, 2 | BRF_PRG | BRF_ESS }, //  1 user1
	{ "bcsv0004f02.u2",		0x1000000, 0x0e799845, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "bcsv0004f03.u3",		0x1000000, 0x659e2d17, 2 | BRF_PRG | BRF_ESS }, //  3
};

STDROMPICKEXT(crysking, crysking, crysbios)
STD_ROM_FN(crysking)

#define WORD_XOR_LE(a) (a)

static void CryskingPatch()
{
	UINT16 *Rom = (UINT16*)DrvMainROM;

	Rom[WORD_XOR_LE(0x7bb6/2)] = 0xDF01;
	Rom[WORD_XOR_LE(0x7bb8/2)] = 0x9C00;

	Rom[WORD_XOR_LE(0x976a/2)] = 0x901C;
	Rom[WORD_XOR_LE(0x976c/2)] = 0x9001;

	Rom[WORD_XOR_LE(0x8096/2)] = 0x90FC;
	Rom[WORD_XOR_LE(0x8098/2)] = 0x9001;

	Rom[WORD_XOR_LE(0x8a52/2)] = 0x4000;	//NOP
	Rom[WORD_XOR_LE(0x8a54/2)] = 0x403c;	//NOP
}

static INT32 CryskingInit()
{
	return DrvInit(3, CryskingPatch);
}

struct BurnDriver BurnDrvCrysking = {
	"crysking", NULL, "crysbios", NULL, "2001",
	"The Crystal of Kings\0", NULL, "BrezzaSoft", "Crystal System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, cryskingRomInfo, cryskingRomName, NULL, NULL, CrystalInputInfo, CrystalDIPInfo,
	CryskingInit, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Evolution Soccer

static struct BurnRomInfo evosoccRomDesc[] = {
	{ "bcsv0001u01",		0x1000000, 0x2581a0ea, 1 }, //  0 user1
	{ "bcsv0001u02",		0x1000000, 0x47ef1794, 1 }, //  1
	{ "bcsv0001u03",		0x1000000, 0xf396a2ec, 1 }, //  2
};

STDROMPICKEXT(evosocc, evosocc, crysbios)
STD_ROM_FN(evosocc)

static void EvosoccPatch()
{
	UINT16 *Rom = (UINT16*)DrvMainROM;
	Rom += 0x1000000 * 2 / 2;

	Rom[WORD_XOR_LE(0x97388E/2)] = 0x90FC;	//PUSH R2..R7
	Rom[WORD_XOR_LE(0x973890/2)] = 0x9001;	//PUSH R0

	Rom[WORD_XOR_LE(0x971058/2)] = 0x907C;	//PUSH R2..R6
	Rom[WORD_XOR_LE(0x971060/2)] = 0x9001; //PUSH R0

	Rom[WORD_XOR_LE(0x978036/2)] = 0x900C;	//PUSH R2-R3
	Rom[WORD_XOR_LE(0x978038/2)] = 0x8303;	//LD    (%SP,0xC),R3

	Rom[WORD_XOR_LE(0x974ED0/2)] = 0x90FC;	//PUSH R7-R6-R5-R4-R3-R2
	Rom[WORD_XOR_LE(0x974ED2/2)] = 0x9001;	//PUSH R0
}

static INT32 EvosoccInit()
{
	return DrvInit(3, EvosoccPatch);
}

struct BurnDriver BurnDrvEvosocc = {
	"evosocc", NULL, "crysbios", NULL, "2001",
	"Evolution Soccer\0", NULL, "Evoga", "Crystal System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, evosoccRomInfo, evosoccRomName, NULL, NULL, CrystalInputInfo, CrystalDIPInfo,
	EvosoccInit, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};
