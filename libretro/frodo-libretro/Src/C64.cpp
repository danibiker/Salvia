/*
 *  C64.cpp - Put the pieces together
 *
 *  Frodo Copyright (C) Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <libretro.h>

#include "sysdeps.h"

#include "C64.h"
#include "CPUC64.h"
#include "CPU1541.h"
#include "VIC.h"
#include "SID.h"
#include "CIA.h"
#include "REU.h"
#include "IEC.h"
#include "1541job.h"
#include "Display.h"
#include "Prefs.h"

#include "main.h"

#ifndef NO_LIBCO
#include "libco.h"
#endif

#ifdef FRODO_SC
bool IsFrodoSC = true;
#else
bool IsFrodoSC = false;
#endif

/* Forward declarations */
extern "C" {
RFILE* rfopen(const char *path, const char *mode);
int64_t rfseek(RFILE* stream, int64_t offset, int origin);
int64_t rftell(RFILE* stream);
int rfclose(RFILE* stream);
int64_t rfread(void* buffer,
   size_t elem_size, size_t elem_count, RFILE* stream);
int64_t rfwrite(void const* buffer,
   size_t elem_size, size_t elem_count, RFILE* stream);
int rfgetc(RFILE* stream);
int rfputc(int character, RFILE * stream);
int rfprintf(RFILE * stream, const char * format, ...);
}
extern retro_input_state_t input_state_cb;
#ifndef NO_LIBCO
extern cothread_t mainThread;
extern cothread_t emuThread;
#endif
extern int pauseg,retro_quit;
extern void pause_select(void);
extern int SHOWKEY;

/*
 *  Constructor: Allocate objects and memory
 */

C64::C64()
{
   unsigned i, j;
	uint8 *p;

	quit_thyself   = false;

	// System-dependent things
	c64_ctor1();

	// Open display
	TheDisplay     = new C64Display(this);

	// Allocate RAM/ROM memory
	RAM            = new uint8[C64_RAM_SIZE];
	Basic          = new uint8[BASIC_ROM_SIZE];
	Kernal         = new uint8[KERNAL_ROM_SIZE];
	Char           = new uint8[CHAR_ROM_SIZE];
	Color          = new uint8[COLOR_RAM_SIZE];
	RAM1541        = new uint8[DRIVE_RAM_SIZE];
	ROM1541        = new uint8[DRIVE_ROM_SIZE];

	// Create the chips
	TheCPU         = new MOS6510(this, RAM, Basic, Kernal, Char, Color);

	TheJob1541     = new Job1541(RAM1541);
	TheCPU1541     = new MOS6502_1541(
         this, TheJob1541, TheDisplay, RAM1541, ROM1541);

	TheVIC         = TheCPU->TheVIC  = new MOS6569(this, TheDisplay, TheCPU, RAM, Char, Color);
	TheSID         = TheCPU->TheSID  = new MOS6581(this);
	TheCIA1        = TheCPU->TheCIA1 = new MOS6526_1(TheCPU, TheVIC);
	TheCIA2        = TheCPU->TheCIA2 = TheCPU1541->TheCIA2 = new MOS6526_2(TheCPU, TheVIC, TheCPU1541);
	TheIEC         = TheCPU->TheIEC = new IEC(TheDisplay);
	TheREU         = TheCPU->TheREU = new REU(TheCPU);

	// Initialize RAM with powerup pattern
	p = RAM;
	for (i = 0; i <512; i++)
   {
      for (j = 0; j < 64; j++)
         *p++ = 0;
      for (j = 0; j < 64; j++)
         *p++ = 0xff;
   }

	// Initialize color RAM with random values
	p = Color;
	for (i = 0; i < COLOR_RAM_SIZE; i++)
		*p++ = rand() & 0x0f;

	// Clear 1541 RAM
	memset(RAM1541, 0, DRIVE_RAM_SIZE);

	joykey = 0xff;

	CycleCounter = 0;

	// System-dependent things
	c64_ctor2();
}


/*
 *  Destructor: Delete all objects
 */

C64::~C64()
{
	delete TheJob1541;
	delete TheREU;
	delete TheIEC;
	delete TheCIA2;
	delete TheCIA1;
	delete TheSID;
	delete TheVIC;
	delete TheCPU1541;
	delete TheCPU;
	delete TheDisplay;

	delete[] RAM;
	delete[] Basic;
	delete[] Kernal;
	delete[] Char;
	delete[] Color;
	delete[] RAM1541;
	delete[] ROM1541;

	c64_dtor();
}


/*
 *  Reset C64
 */

void C64::Reset(void)
{
	TheCPU->AsyncReset();
	TheCPU1541->AsyncReset();
	TheSID->Reset();
	TheCIA1->Reset();
	TheCIA2->Reset();
	TheIEC->Reset();
}


/*
 *  NMI C64
 */

void C64::NMI(void)
{
	TheCPU->AsyncNMI();
}


/*
 *  The preferences have changed. prefs is a pointer to the new
 *   preferences, ThePrefs still holds the previous ones.
 *   The emulation must be in the paused state!
 */

void C64::NewPrefs(Prefs *prefs)
{
	PatchKernal(prefs->FastReset, prefs->Emul1541Proc);

	TheDisplay->NewPrefs(prefs);

	TheIEC->NewPrefs(prefs);
	TheJob1541->NewPrefs(prefs);

	TheREU->NewPrefs(prefs);
	TheSID->NewPrefs(prefs);

	// Reset 1541 processor if turned on
	if (!ThePrefs.Emul1541Proc && prefs->Emul1541Proc)
		TheCPU1541->AsyncReset();
}


/*
 *  Patch kernal IEC routines
 */

void C64::PatchKernal(bool fast_reset, bool emul_1541_proc)
{
	if (fast_reset)
   {
      Kernal[0x1d84] = 0xa0;
      Kernal[0x1d85] = 0x00;
   }
   else
   {
      Kernal[0x1d84] = orig_kernal_1d84;
      Kernal[0x1d85] = orig_kernal_1d85;
   }

	if (emul_1541_proc)
   {
      Kernal[0x0d40] = 0x78;
      Kernal[0x0d41] = 0x20;
      Kernal[0x0d23] = 0x78;
      Kernal[0x0d24] = 0x20;
      Kernal[0x0d36] = 0x78;
      Kernal[0x0d37] = 0x20;
      Kernal[0x0e13] = 0x78;
      Kernal[0x0e14] = 0xa9;
      Kernal[0x0def] = 0x78;
      Kernal[0x0df0] = 0x20;
      Kernal[0x0dbe] = 0xad;
      Kernal[0x0dbf] = 0x00;
      Kernal[0x0dcc] = 0x78;
      Kernal[0x0dcd] = 0x20;
      Kernal[0x0e03] = 0x20;
      Kernal[0x0e04] = 0xbe;
   } else {
		Kernal[0x0d40] = 0xf2;	// IECOut
		Kernal[0x0d41] = 0x00;
		Kernal[0x0d23] = 0xf2;	// IECOutATN
		Kernal[0x0d24] = 0x01;
		Kernal[0x0d36] = 0xf2;	// IECOutSec
		Kernal[0x0d37] = 0x02;
		Kernal[0x0e13] = 0xf2;	// IECIn
		Kernal[0x0e14] = 0x03;
		Kernal[0x0def] = 0xf2;	// IECSetATN
		Kernal[0x0df0] = 0x04;
		Kernal[0x0dbe] = 0xf2;	// IECRelATN
		Kernal[0x0dbf] = 0x05;
		Kernal[0x0dcc] = 0xf2;	// IECTurnaround
		Kernal[0x0dcd] = 0x06;
		Kernal[0x0e03] = 0xf2;	// IECRelease
		Kernal[0x0e04] = 0x07;
	}

	// 1541
	ROM1541[0x2ae4]   = 0xea;		// Don't check ROM checksum
	ROM1541[0x2ae5]   = 0xea;
	ROM1541[0x2ae8]   = 0xea;
	ROM1541[0x2ae9]   = 0xea;
	ROM1541[0x2c9b]   = 0xf2;		// DOS idle loop
	ROM1541[0x2c9c]   = 0x00;
	ROM1541[0x3594]   = 0x20;		// Write sector
	ROM1541[0x3595]   = 0xf2;
	ROM1541[0x3596]   = 0xf5;
	ROM1541[0x3597]   = 0xf2;
	ROM1541[0x3598]   = 0x01;
	ROM1541[0x3b0c]   = 0xf2;		// Format track
	ROM1541[0x3b0d]   = 0x02;
}

/*  Save RAM contents */
void C64::SaveRAM(char *filename)
{
   RFILE *f;
   if (!(f = rfopen(filename, "wb")))
      return;
   rfwrite((void*)RAM, 1, 0x10000, f);
   rfwrite((void*)Color, 1, 0x400, f);
   if (ThePrefs.Emul1541Proc)
      rfwrite((void*)RAM1541, 1, 0x800, f);
   rfclose(f);
}

/*
 *  Save CPU state to snapshot
 *
 *  0: Error
 *  1: OK
 *  -1: Instruction not completed
 */
int C64::SaveCPUState(RFILE *f)
{
   int i;
	MOS6510State state;
	TheCPU->GetState(&state);

	if (!state.instruction_complete)
		return -1;

	i      = rfwrite(RAM, 0x10000, 1, f);
	i     += rfwrite(Color, 0x400, 1, f);
	i     += rfwrite((void*)&state, sizeof(state), 1, f);

	return i == 3;
}


/*
 *  Load CPU state from snapshot
 */

bool C64::LoadCPUState(RFILE *f)
{
	MOS6510State state;
	int i  = rfread(RAM, 0x10000, 1, f);
	i     += rfread(Color, 0x400, 1, f);
	i     += rfread((void*)&state, sizeof(state), 1, f);

	if (i == 3)
   {
      TheCPU->SetState(&state);
      return true;
   }
   return false;
}


/*
 *  Save 1541 state to snapshot
 *
 *  0: Error
 *  1: OK
 *  -1: Instruction not completed
 */

int C64::Save1541State(RFILE *f)
{
   int i;
	MOS6502State state;
	TheCPU1541->GetState(&state);

	if (!state.idle && !state.instruction_complete)
		return -1;

	i  = rfwrite(RAM1541, 0x800, 1, f);
	i += rfwrite((void*)&state, sizeof(state), 1, f);
	return i == 2;
}


/*
 *  Load 1541 state from snapshot
 */

bool C64::Load1541State(RFILE *f)
{
	MOS6502State state;
	int i = rfread(RAM1541, 0x800, 1, f);
	i    += rfread((void*)&state, sizeof(state), 1, f);

	if (i == 2)
   {
      TheCPU1541->SetState(&state);
      return true;
   }
   return false;
}


/*
 *  Save VIC state to snapshot
 */

bool C64::SaveVICState(RFILE *f)
{
	MOS6569State state;
	TheVIC->GetState(&state);
	return rfwrite((void*)&state, sizeof(state), 1, f) == 1;
}


/*
 *  Load VIC state from snapshot
 */

bool C64::LoadVICState(RFILE *f)
{
	MOS6569State state;
	if (rfread((void*)&state, sizeof(state), 1, f) == 1)
   {
      TheVIC->SetState(&state);
      return true;
   }
   return false;
}


/*
 *  Save SID state to snapshot
 */

bool C64::SaveSIDState(RFILE *f)
{
	MOS6581State state;
	TheSID->GetState(&state);
	return rfwrite((void*)&state, sizeof(state), 1, f) == 1;
}


/*
 *  Load SID state from snapshot
 */

bool C64::LoadSIDState(RFILE *f)
{
	MOS6581State state;
	if (rfread((void*)&state, sizeof(state), 1, f) == 1)
   {
      TheSID->SetState(&state);
      return true;
   }
   return false;
}


/*
 *  Save CIA states to snapshot
 */

bool C64::SaveCIAState(RFILE *f)
{
	MOS6526State state;
	TheCIA1->GetState(&state);
	if (rfwrite((void*)&state, sizeof(state), 1, f) == 1)
   {
      TheCIA2->GetState(&state);
      return rfwrite((void*)&state, sizeof(state), 1, f) == 1;
   }
   return false;
}


/*
 *  Load CIA states from snapshot
 */

bool C64::LoadCIAState(RFILE *f)
{
	MOS6526State state;
	if (rfread((void*)&state, sizeof(state), 1, f) == 1)
   {
      TheCIA1->SetState(&state);
      if (rfread((void*)&state, sizeof(state), 1, f) == 1)
      {
         TheCIA2->SetState(&state);
         return true;
      }
   }
   return false;
}


/*
 *  Save 1541 GCR state to snapshot
 */

bool C64::Save1541JobState(RFILE *f)
{
	Job1541State state;
	TheJob1541->GetState(&state);
	return rfwrite((void*)&state, sizeof(state), 1, f) == 1;
}


/*
 *  Load 1541 GCR state from snapshot
 */

bool C64::Load1541JobState(RFILE *f)
{
	Job1541State state;
	if (rfread((void*)&state, sizeof(state), 1, f) == 1)
   {
      TheJob1541->SetState(&state);
      return true;
   }
   return false;
}


#define SNAPSHOT_HEADER "FrodoSnapshot"
#define SNAPSHOT_1541 1

#define ADVANCE_CYCLES	\
	TheVIC->EmulateCycle(); \
	TheCIA1->EmulateCycle(); \
	TheCIA2->EmulateCycle(); \
	TheCPU->EmulateCycle(); \
	if (ThePrefs.Emul1541Proc) { \
		TheCPU1541->CountVIATimers(1); \
		if (!TheCPU1541->Idle) \
			TheCPU1541->EmulateCycle(); \
	}


/*
 *  Save snapshot (emulation must be paused and in VBlank)
 *
 *  To be able to use SC snapshots with SL, SC snapshots are made thus that no
 *  partially dealt with instructions are saved. Instead all devices are advanced
 *  cycle by cycle until the current instruction has been finished. The number of
 *  cycles this takes is saved in the snapshot and will be reconstructed if the
 *  snapshot is loaded into FrodoSC again.
 */

void C64::SaveSnapshot(char *filename)
{
   RFILE *f;
   uint8 flags;
   uint8 delay;
   int stat;
   if (!(f = rfopen(filename, "wb")))
      return;

   rfprintf(f, "%s%c", SNAPSHOT_HEADER, 10);
   rfputc(0, f);	// Version number 0
   flags = 0;
   if (ThePrefs.Emul1541Proc)
      flags |= SNAPSHOT_1541;
   rfputc(flags, f);
   SaveVICState(f);
   SaveSIDState(f);
   SaveCIAState(f);

#ifdef FRODO_SC
   delay = 0;
   do
   {
      if ((stat = SaveCPUState(f)) == -1)
      {
         // -1 -> Instruction not finished yet
         ADVANCE_CYCLES;	 // Advance everything by one cycle
         delay++;
      }
   } while (stat == -1);
   rfputc(delay, f);	// Number of cycles the 
   // saved CPUC64 lags behind the previous chips
#else
   SaveCPUState(f);
   rfputc(0, f);		// No delay
#endif

   if (ThePrefs.Emul1541Proc)
   {
      rfwrite(ThePrefs.DrivePath[0], 256, 1, f);
#ifdef FRODO_SC
      delay = 0;
      do
      {
         if ((stat = Save1541State(f)) == -1)
         {
            ADVANCE_CYCLES;
            delay++;
         }
      } while (stat == -1);
      rfputc(delay, f);
#else
      Save1541State(f);
      rfputc(0, f);	// No delay
#endif
      Save1541JobState(f);
   }
   rfclose(f);
}


/*
 *  Load snapshot (emulation must be paused and in VBlank)
 */

bool C64::LoadSnapshot(char *filename)
{
	RFILE *f;
	if ((f = rfopen(filename, "rb")))
   {
      uint8 delay, i;
      char Header[] = SNAPSHOT_HEADER;
      char       *b = Header, c = 0;

      /* For some reason memcmp()/strcmp() 
         and so forth utterly fail here. */
      while (*b > 32)
      {
         if ((c = rfgetc(f)) != *b++)
         {
            b = NULL;
            break;
         }
      }

      if (b)
      {
         uint8 flags;
         bool error = false;
#ifndef FRODO_SC
         long vicptr;	// File offset of VIC data
#endif

         while (c != 10)
            c = rfgetc(f);	// Shouldn't be necessary
         if (rfgetc(f) != 0)
         {
            rfclose(f);
            return false;
         }
         flags  = rfgetc(f);
#ifndef FRODO_SC
         vicptr = rftell(f);
#endif

         error |= !LoadVICState(f);
         error |= !LoadSIDState(f);
         error |= !LoadCIAState(f);
         error |= !LoadCPUState(f);

         delay  = rfgetc(f);	// Number of cycles the 6510 is ahead of the previous chips
#ifdef FRODO_SC
         // Make the other chips "catch up" with the 6510
         for (i=0; i<delay; i++)
         {
            TheVIC->EmulateCycle();
            TheCIA1->EmulateCycle();
            TheCIA2->EmulateCycle();
         }
#endif
         if ((flags & SNAPSHOT_1541) != 0)
         {
            Prefs *prefs         = new Prefs(ThePrefs);
            // First switch on emulation
            error               |= (rfread(prefs->DrivePath[0], 256, 1, f) 
                  != 1);
            prefs->Emul1541Proc  = true;
            NewPrefs(prefs);
            ThePrefs             = *prefs;
            delete prefs;

            // Then read the context
            error               |= !Load1541State(f);

            delay                = rfgetc(f);	
                                 // Number of cycles 
                                 // the 6502 is ahead of the previous chips
#ifdef FRODO_SC
            // Make the other chips "catch up" with the 6502
            for (i = 0; i < delay; i++)
            {
               TheVIC->EmulateCycle();
               TheCIA1->EmulateCycle();
               TheCIA2->EmulateCycle();
               TheCPU->EmulateCycle();
            }
#endif
            Load1541JobState(f);
         }
         else if (ThePrefs.Emul1541Proc)
         {
            // No emulation in snapshot, but currently active?
            Prefs *prefs        = new Prefs(ThePrefs);
            prefs->Emul1541Proc = false;
            NewPrefs(prefs);
            ThePrefs            = *prefs;
            delete prefs;
         }

#ifndef FRODO_SC
         rfseek(f, vicptr, SEEK_SET);
         LoadVICState(f);	// Load VIC data twice 
                           // in SL (is REALLY necessary sometimes!)
#endif
         rfclose(f);

         if (error)
         {
            Reset();
            return false;
         }
         return true;
      }
      rfclose(f);
   }
   return false;
}

/*
 *  C64_x.i - Put the pieces together, X specific stuff
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 *  Unix stuff by Bernd Schmidt/Lutz Vieweg
 */

/*  Constructor, system-dependent things */
void C64::c64_ctor1(void)
{

}

void C64::c64_ctor2(void)
{
}


/*  Destructor, system-dependent things */
void C64::c64_dtor(void)
{
}

/*  Start main emulation thread */
void C64::Run(void)
{
	// Reset chips
	TheCPU->Reset();
	TheSID->Reset();
	TheCIA1->Reset();
	TheCIA2->Reset();
	TheCPU1541->Reset();

	// Patch kernal IEC routines
	orig_kernal_1d84 = Kernal[0x1d84];
	orig_kernal_1d85 = Kernal[0x1d85];
	PatchKernal(ThePrefs.FastReset, ThePrefs.Emul1541Proc);

	quit_thyself     = false;
#ifndef NO_LIBCO
	thread_func();
#endif
}

/*  Vertical blank: Poll keyboard and joysticks, update window */
void C64::VBlank(bool draw_frame)
{
   // Poll keyboard
   TheDisplay->PollKeyboard(
         TheCIA1->KeyMatrix, TheCIA1->RevMatrix, &joykey);

   if (TheDisplay->quit_requested)
      quit_thyself    = true;

   // Poll joysticks
   TheCIA1->Joystick1 = poll_joystick(0);
   TheCIA1->Joystick2 = poll_joystick(1);

   if (ThePrefs.JoystickSwap)
   {
      uint8 tmp          = TheCIA1->Joystick1;
      TheCIA1->Joystick1 = TheCIA1->Joystick2;
      TheCIA1->Joystick2 = tmp;
   }

   // Joystick keyboard emulation
   if (TheDisplay->NumLock())
      TheCIA1->Joystick1 &= joykey;
   else
      TheCIA1->Joystick2 &= joykey;

   // Count TOD clocks
   TheCIA1->CountTOD();
   TheCIA2->CountTOD();

   TheDisplay->Update();

   if (pauseg == 1)
      pause_select();
   if (retro_quit == 1)
      quit_thyself = true;
#ifndef NO_LIBCO
   co_switch(mainThread);
#endif
}

/*  Poll joystick port, return CIA mask */
uint8 C64::poll_joystick(int port)
{
	if (SHOWKEY != 1)
   {
      uint8 j = 0xff;
      if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_RIGHT))
         j &= 0xf7;
      if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_LEFT))
         j &= 0xfb;
      if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_DOWN))
         j &= 0xfd;
      if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_UP))
         j &= 0xfe;
      if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_A))
         j &= 0xef;
      return j;
   }

	return 0xff;
}


/* The emulation's main loop */
void C64::thread_func(void)
{
	int linecnt = 0;

#ifdef FRODO_SC
#ifndef NO_LIBCO
	while (!quit_thyself)
#endif
    {
		// The order of calls is important here
		if (TheVIC->EmulateCycle())
			TheSID->EmulateLine();
		TheCIA1->CheckIRQs();
		TheCIA2->CheckIRQs();
		TheCIA1->EmulateCycle();
		TheCIA2->EmulateCycle();
		TheCPU->EmulateCycle();

		if (ThePrefs.Emul1541Proc)
      {
         TheCPU1541->CountVIATimers(1);
         if (!TheCPU1541->Idle)
            TheCPU1541->EmulateCycle();
      }
		CycleCounter++;
#else
#ifndef NO_LIBCO
	while (!quit_thyself)
#endif
 	{
		// The order of calls is important here
		int cycles = TheVIC->EmulateLine();
		TheSID->EmulateLine();
#if !PRECISE_CIA_CYCLES
		TheCIA1->EmulateLine(ThePrefs.CIACycles);
		TheCIA2->EmulateLine(ThePrefs.CIACycles);
#endif

		if (ThePrefs.Emul1541Proc)
      {
         int cycles_1541 = ThePrefs.FloppyCycles;
         TheCPU1541->CountVIATimers(cycles_1541);

         if (!TheCPU1541->Idle)
         {
            // 1541 processor active, alternately execute
            //  6502 and 6510 instructions until both have
            //  used up their cycles
            while (  cycles >= 0 
                  || cycles_1541 >= 0)
               if (cycles > cycles_1541)
                  cycles      -= TheCPU->EmulateLine(1);
               else
                  cycles_1541 -= TheCPU1541->EmulateLine(1);
         } else
            TheCPU->EmulateLine(cycles);
      } else
			// 1541 processor disabled, only emulate 6510
			TheCPU->EmulateLine(cycles);
#endif
		linecnt++;
	}
}
