//
// Copyright (c) 2004 K. Wilkins
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from
// the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//

//////////////////////////////////////////////////////////////////////////////
//                       Handy - An Atari Lynx Emulator                     //
//                          Copyright (c) 1996,1997                         //
//                                 K. Wilkins                               //
//////////////////////////////////////////////////////////////////////////////
// System object class                                                      //
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// This class provides the glue to bind of of the emulation objects         //
// together via peek/poke handlers and pass thru interfaces to lower        //
// objects, all control of the emulator is done via this class. Update()    //
// does most of the work and each call emulates one CPU instruction and     //
// updates all of the relevant hardware if required. It must be remembered  //
// that if that instruction involves setting SPRGO then, it will cause a    //
// sprite painting operation and then a corresponding update of all of the  //
// hardware which will usually involve recursive calls to Update, see       //
// Mikey SPRGO code for more details.                                       //
//                                                                          //
//    K. Wilkins                                                            //
// August 1997                                                              //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
// Revision History:                                                        //
// -----------------                                                        //
//                                                                          //
// 01Aug1997 KW Document header added & class documented.                   //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

#define SYSTEM_CPP

#include "../lynx/system.h"
#include "../mednafen-endian.h"

#include "../general.h"
#include "../mempatcher.h"
#include "../md5.h"
#include "../settings.h"

CSystem::CSystem(MDFNFILE *fp, const char *bios_path)
	:mCart(NULL),
	mRom(NULL),
	mMemMap(NULL),
	mRam(NULL),
	mCpu(NULL),
	mMikie(NULL),
	mSusie(NULL)
{
	mFileType=HANDY_FILETYPE_ILLEGAL;

	char clip[11];
	file_read(fp, clip, 11, 1);
	file_seek(fp, 0, SEEK_SET);
	clip[4]=0;
	clip[10]=0;

	if(!strcmp(&clip[6],"BS93"))
     mFileType=HANDY_FILETYPE_HOMEBREW;
	else if(!strcmp(&clip[0],"LYNX"))
     mFileType=HANDY_FILETYPE_LNX;
	else if(fp->size==128*1024 || fp->size==256*1024 || fp->size==512*1024)
		/* Invalid Cart (type). but 128/256/512k size -> set to RAW and try to load raw rom image */
		mFileType=HANDY_FILETYPE_RAW;

	MDFNMP_Init(65536, 1);

	// Create the system objects that we'll use

	// Attempt to load the cartridge errors caught above here...

	mRom = new CRom(bios_path);

	// An exception from this will be caught by the level above

	switch(mFileType)
	{
		case HANDY_FILETYPE_RAW:
		case HANDY_FILETYPE_LNX:
			mCart = new CCart(fp);
			mRam = new CRam(NULL);
			break;
		case HANDY_FILETYPE_HOMEBREW:
			mCart = new CCart(NULL);
			mRam = new CRam(fp);
			break;
		case HANDY_FILETYPE_SNAPSHOT:
		case HANDY_FILETYPE_ILLEGAL:
		default:
			mCart = new CCart(NULL);
			mRam = new CRam(NULL);
			break;
	}

	// These can generate exceptions

	mMikie = new CMikie(*this);
	mSusie = new CSusie(*this);

	// Instantiate the memory map handler
	mMemMap = new CMemMap(*this);

	// Now the handlers are set we can instantiate the CPU as is will use handlers on reset
	mCpu = new C65C02(*this);

	// Now init is complete do a reset, this will cause many things to be reset twice
	// but what the hell, who cares, I don't.....
	Reset();
}

CSystem::~CSystem()
{
	// Cleanup all our objects

	if(mCart!=NULL) delete mCart;
	if(mRom!=NULL) delete mRom;
	if(mRam!=NULL) delete mRam;
	if(mCpu!=NULL) delete mCpu;
	if(mMikie!=NULL) delete mMikie;
	if(mSusie!=NULL) delete mSusie;
	if(mMemMap!=NULL) delete mMemMap;
}

void CSystem::Reset(void)
{
	mMikie->startTS -= gSystemCycleCount;
	gSystemCycleCount=0;
	gNextTimerEvent=0;
	gCPUBootAddress=0;
	gSystemIRQ=false;
	gSystemNMI=false;
	gSystemCPUSleep=false;
	gSystemHalt=false;
	gSuzieDoneTime = 0;

	mMemMap->Reset();
	mCart->Reset();
	mRom->Reset();
	mRam->Reset();
	mMikie->Reset();
	mSusie->Reset();
	mCpu->Reset();

	// Homebrew hashup

	if(mFileType==HANDY_FILETYPE_HOMEBREW)
	{
		mMikie->PresetForHomebrew();

		C6502_REGS regs;
		mCpu->GetRegs(regs);
		regs.PC=(uint16)gCPUBootAddress;
		mCpu->SetRegs(regs);
	}
}

// Somewhat of a hack to make sure undrawn lines are black.
bool LynxLineDrawn[256];

CSystem *lynxie = NULL;

static void Cleanup(void)
{
 if(lynxie)
 {
  delete lynxie;
  lynxie = NULL;
 }
}

void Load(MDFNFILE *fp, const char *bios_path)
{
 lynxie = new CSystem(fp, bios_path);

 switch(lynxie->CartGetRotate())
 {
  case CART_ROTATE_LEFT:
   MDFNGameInfo->rotated = MDFN_ROTATE270;
   break;

  case CART_ROTATE_RIGHT:
   MDFNGameInfo->rotated = MDFN_ROTATE90;
   break;
 }

 if(MDFN_GetSettingB("lynx.lowpass"))
  lynxie->mMikie->miksynth.treble_eq(-35);
 else
  lynxie->mMikie->miksynth.treble_eq(0);
}

void CloseGame(void)
{
 Cleanup();
}

static uint8 *chee;
void Emulate(EmulateSpecStruct *espec)
{
 espec->DisplayRect.x = 0;
 espec->DisplayRect.y = 0;
 espec->DisplayRect.w = 160;
 espec->DisplayRect.h = 102;

 if(espec->SoundFormatChanged)
 {
  lynxie->mMikie->mikbuf.set_sample_rate(espec->SoundRate ? espec->SoundRate : 44100, 60);
  lynxie->mMikie->mikbuf.clock_rate((long int)(16000000 / 4));
  lynxie->mMikie->mikbuf.bass_freq(60);
  lynxie->mMikie->miksynth.volume(0.50);
 }

 uint16 butt_data = chee[0] | (chee[1] << 8);

 lynxie->SetButtonData(butt_data);

 MDFNMP_ApplyPeriodicCheats();

 memset(LynxLineDrawn, 0, sizeof(LynxLineDrawn[0]) * 102);

 lynxie->mMikie->mpSkipFrame = espec->skip;
 lynxie->mMikie->mpDisplayCurrent = espec->surface;
 lynxie->mMikie->mpDisplayCurrentLine = 0;
 lynxie->mMikie->startTS = gSystemCycleCount;

 while(lynxie->mMikie->mpDisplayCurrent && (gSystemCycleCount - lynxie->mMikie->startTS) < 700000)
  lynxie->Update();

 {
	 // FIXME, we should integrate this into mikie.*
	 uint32 color_black;
	 if (espec->surface->bpp == 16)
	 {
#if defined(ABGR1555)
		 color_black = MAKECOLOR_15_1(30, 30, 30, 0);
#else
		 color_black = MAKECOLOR_16(30, 30, 30, 0);
#endif
		 for (int y = 0; y < 102; y++)
		 {
			 uint16 *row = espec->surface->pixels + y * espec->surface->pitch;

			 if (!LynxLineDrawn[y])
			 {
				 for (int x = 0; x < 160; x++)
					 row[x] = color_black;
			 }
		 }
	 }
	 else if (espec->surface->bpp == 32)
	 {
		 color_black = MAKECOLOR_32(30, 30, 30, 0);
		 for (int y = 0; y < 102; y++)
		 {
			 uint32 *row = (uint32*)espec->surface->pixels + y * espec->surface->pitch;

			 if (!LynxLineDrawn[y])
			 {
				 for (int x = 0; x < 160; x++)
					 row[x] = color_black;
			 }
		 }
	 }
 }

 if(espec->SoundBuf)
 {
  lynxie->mMikie->mikbuf.end_frame((gSystemCycleCount - lynxie->mMikie->startTS) >> 2);
  espec->SoundBufSize = lynxie->mMikie->mikbuf.read_samples(espec->SoundBuf, espec->SoundBufMaxSize) / 2; // divide by nr audio chn
 }
 else
  espec->SoundBufSize = 0;
}

void SetInput(unsigned port, const char *type, uint8 *ptr)
{
 chee = (uint8 *)ptr;
}

static void TransformInput(void)
{
 if(MDFN_GetSettingB("lynx.rotateinput"))
 {
  static const unsigned bp[4] = { 4, 6, 5, 7 };
  const unsigned offs = MDFNGameInfo->rotated;
  uint16 butt_data = MDFN_de16lsb(chee);

  butt_data = (butt_data & 0xFF0F) |
	      (((butt_data >> bp[0]) & 1) << bp[(0 + offs) & 3]) |
	      (((butt_data >> bp[1]) & 1) << bp[(1 + offs) & 3]) |
	      (((butt_data >> bp[2]) & 1) << bp[(2 + offs) & 3]) |
	      (((butt_data >> bp[3]) & 1) << bp[(3 + offs) & 3]);
  MDFN_en16lsb(chee, butt_data);
 }
}

int StateAction(StateMem *sm, int load, int data_only)
{
 SFORMAT SystemRegs[] =
 {
	SFVAR(gSuzieDoneTime),
        SFVAR(gSystemCycleCount),
        SFVAR(gNextTimerEvent),
        SFVAR(gCPUBootAddress),
        SFVAR(gSystemIRQ),
        SFVAR(gSystemNMI),
        SFVAR(gSystemCPUSleep),
        SFVAR(gSystemHalt),
	SFARRAYN(lynxie->GetRamPointer(), RAM_SIZE, "RAM"),
	SFEND
 };

 int ret = MDFNSS_StateAction(sm, load, data_only, SystemRegs, "SYST", false);
 ret &= lynxie->mSusie->StateAction(sm, load, data_only);
 ret &= lynxie->mMemMap->StateAction(sm, load, data_only);
 ret &= lynxie->mCart->StateAction(sm, load, data_only);
 ret &= lynxie->mMikie->StateAction(sm, load, data_only);
 ret &= lynxie->mCpu->StateAction(sm, load, data_only);
 return ret;
}

void DoSimpleCommand(int cmd)
{
 switch(cmd)
 {
  case MDFN_MSC_POWER:
  case MDFN_MSC_RESET: lynxie->Reset(); break;
 }
}

static const InputDeviceInputInfoStruct IDII[] =
{
 { "a", "A (outer)", 8, IDIT_BUTTON_CAN_RAPID, NULL },
 { "b", "B (inner)", 7, IDIT_BUTTON_CAN_RAPID, NULL },
 { "option_2", "Option 2 (lower)", 5, IDIT_BUTTON_CAN_RAPID, NULL },
 { "option_1", "Option 1 (upper)", 4, IDIT_BUTTON_CAN_RAPID, NULL },

 { "left", "LEFT ←", 	/*VIRTB_DPAD0_L,*/ 2, IDIT_BUTTON, "right",		{ "up", "right", "down" } },
 { "right", "RIGHT →", 	/*VIRTB_DPAD0_R,*/ 3, IDIT_BUTTON, "left", 		{ "down", "left", "up" } },
 { "up", "UP ↑", 	/*VIRTB_DPAD0_U,*/ 0, IDIT_BUTTON, "down",		{ "right", "down", "left" } },
 { "down", "DOWN ↓", 	/*VIRTB_DPAD0_D,*/ 1, IDIT_BUTTON, "up", 		{ "left", "up", "right" } },

 { "pause", "PAUSE", 6, IDIT_BUTTON, NULL },
};

static InputDeviceInfoStruct InputDeviceInfo[] =
{
 {
  "gamepad",
  "Gamepad",
  NULL,
  NULL,
  sizeof(IDII) / sizeof(InputDeviceInputInfoStruct),
  IDII,
 }
};

static const InputPortInfoStruct PortInfo[] =
{
 { "builtin", "Built-In", sizeof(InputDeviceInfo) / sizeof(InputDeviceInfoStruct), InputDeviceInfo, 0 }
};

static InputInfoStruct InputInfo =
{
 sizeof(PortInfo) / sizeof(InputPortInfoStruct),
 PortInfo
};

MDFNGI EmulatedLynx =
{
 160,   // lcm_width
 102,   // lcm_height
 NULL,  // Dummy


 160,	// Nominal width
 102,	// Nominal height

 160,	// Framebuffer width
 102,	// Framebuffer height
};

