/*
 *  main.cpp - Main program
 *
 *  Frodo (C) 1994-1997,2002-2005 Christian Bauer
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

#include <string.h>
#if defined(_MSC_VER)
    #include <time/rtime.h> 
    // Esto te dar� acceso a las funciones multiplataforma de tiempo de Libretro
#else
#include <sys/time.h> 
#endif

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>   /* getcwd */
#endif
#include <sys/stat.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include "Version.h"

#include "core-log.h"
#ifndef NO_LIBCO
#include "libco.h"
#endif

#include "sysdeps.h"

#include "main.h"
#include "C64.h"
#include "Display.h"
#include "Prefs.h"

/* Forward declarations */
int init_graphics(void);

extern "C" {
RFILE* rfopen(const char *path, const char *mode);
int64_t rftell(RFILE* stream);
int rfclose(RFILE* stream);
int64_t rfread(void* buffer,
   size_t elem_size, size_t elem_count, RFILE* stream);
}
#ifndef NO_LIBCO
extern cothread_t mainThread;
extern cothread_t emuThread;
#endif

/* Global variables */
C64 *TheC64 = NULL;		/* Global C64 object */
char AppDirPath[1024];	/* Path of application directory */
char Frodo::device_path[256] = "";
Frodo *the_app;

/* ROM file names */
#ifndef DATADIR
#define DATADIR
#endif

#define BASIC_ROM_FILE DATADIR "Basic ROM"
#define KERNAL_ROM_FILE DATADIR "Kernal ROM"
#define CHAR_ROM_FILE DATADIR "Char ROM"
#define DRIVE_ROM_FILE DATADIR "1541 ROM"

/* Builtin ROMs */
#include "Basic_ROM.h"
#include "Kernal_ROM.h"
#include "Char_ROM.h"
#include "1541_ROM.h"

/* Load C64 ROM files */

bool Frodo::load_rom(const char *which, const char *path,
      uint8 *where, size_t size, const uint8 *builtin)
{
   RFILE *f = rfopen(path, "rb");
   if (f)
   {
      size_t actual = (size_t)rfread(where, 1, size, f);
      rfclose(f);
      if (actual == size)
         return true;
   }
   return false;
}

void Frodo::load_rom_files()
{
	if (!load_rom("Basic", BASIC_ROM_FILE, TheC64->Basic,
            BASIC_ROM_SIZE, builtin_basic_rom))
      memcpy(TheC64->Basic, builtin_basic_rom, BASIC_ROM_SIZE);

	if (!load_rom("Kernal", KERNAL_ROM_FILE, TheC64->Kernal,
            KERNAL_ROM_SIZE, builtin_kernal_rom))
      memcpy(TheC64->Kernal, builtin_kernal_rom, KERNAL_ROM_SIZE);

	if (!load_rom("Char", CHAR_ROM_FILE, TheC64->Char,
            CHAR_ROM_SIZE, builtin_char_rom))
      memcpy(TheC64->Char, builtin_char_rom, CHAR_ROM_SIZE);

	if (!load_rom("1541", DRIVE_ROM_FILE, TheC64->ROM1541,
            DRIVE_ROM_SIZE, builtin_drive_rom))
      memcpy(TheC64->ROM1541, builtin_drive_rom, DRIVE_ROM_SIZE);
}


/*
 *  Create application object and start it
 */

int skel_main(int argc, char **argv)
{
#if defined(_MSC_VER)
    // En Windows (VS2010), combinamos el tiempo en segundos con los ticks del reloj de la CPU
    // Esto genera una semilla altamente variable en cada ejecuci�n sin depender de sys/time.h
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)clock();
    srand(seed);
#else
    // Mantenemos el comportamiento original id�ntico para Linux/macOS
    timeval tv;
    gettimeofday(&tv, NULL);
    srand(tv.tv_usec);
#endif

    if (!init_graphics())
        return 0;

    the_app = new Frodo();
    the_app->ArgvReceived(argc, argv);
    the_app->ReadyToRun();
#ifndef NO_LIBCO
    delete the_app;
    the_app = NULL;
#endif
    return 0;
}

/* Idempotent teardown of the emulator heap objects. Safe to call more than
   once (guards + NULLs), so both the F10-quit path and the reload path
   (retro_deinit) can call it without double-freeing. Deleting TheC64 also
   frees the display surface via ~C64 -> ~C64Display. */
void quit_frodo_emu(void)
{
	if (TheC64)  { delete TheC64;  TheC64  = NULL; }
	if (the_app) { delete the_app; the_app = NULL; }
}

/*
 *  Constructor: Initialize member variables
 */

Frodo::Frodo()
{
	TheC64 = NULL;
}


/* Process command line arguments */
void Frodo::ArgvReceived(int argc, char **argv)
{
	if (argc == 2)
		strncpy(device_path, argv[1], 255);
}

/* Arguments processed, run emulation */
void Frodo::ReadyToRun(void)
{
#if defined (_XBOX)
	strcpy(AppDirPath, "game:\\");
#elif defined (__vita__) || defined(__psp__)
	strcpy(AppDirPath, "/");
#else
	getcwd(AppDirPath, 256);
#endif

	ThePrefs.set_drive8(device_path,0);

	// Create and start C64
	TheC64 = new C64;

	load_rom_files();

#ifndef NO_LIBCO
	co_switch(mainThread); //return mainthread before enter C64thread
#endif

	TheC64->Run();

#ifndef NO_LIBCO
	delete TheC64;
	TheC64 = NULL;
#endif
}

/* Determine whether path name refers to a directory */

bool IsDirectory(const char *path)
{
   return path_is_directory(path);
}

/* Change the disk mounted in drive 8 at runtime, WITHOUT resetting the C64.
   Uses the same live-apply path as the built-in GUI: C64::NewPrefs() recreates
   drive 8 with the new image (IEC::NewPrefs) and only resets the 1541 CPU if
   true-drive emulation is being turned on (not the case here). Called from the
   libretro disk-control callbacks. */
void disk_change_image(const char *path)
{
   if (!TheC64 || !path || !path[0])
      return;

   Prefs p = ThePrefs;
   strncpy(p.DrivePath[0], path, 255);
   p.DrivePath[0][255] = 0;

   TheC64->NewPrefs(&p);   // recreate drive 8 live (no C64 reset)
   ThePrefs = p;
}

/* ---- Core-option mirror + live apply -------------------------------------
   These are written by update_variables() (libretro.cpp) from the libretro
   core options, then applied here. When the machine already exists we apply
   live via C64::NewPrefs() (same path as the built-in GUI); before boot we
   just set ThePrefs, which the C64 constructor reads. NOTE: Joystick1Port/
   Joystick2Port are dead in this port, the pad routing is only JoystickSwap. */
int fopt_sid_engine  = 1;   /* 1 = digital SID, 0 = none (no sound)          */
int fopt_sid_filters = 1;   /* emulate SID filters                           */
int fopt_true_drive  = 0;   /* Emul1541Proc: processor-level 1541 emulation  */
int fopt_fast_reset  = 0;   /* skip RAM test on reset                        */
int fopt_joy_port    = 2;   /* C64 port the controller drives: 2 or 1        */
int fopt_sprite_coll = 1;   /* sprite collision detection                    */
int fopt_reu         = 0;   /* REU size: 0=none,1=128K,2=256K,3=512K         */

void frodo_apply_prefs(void)
{
   Prefs p = ThePrefs;

   p.SIDType          = fopt_sid_engine ? SIDTYPE_DIGITAL : SIDTYPE_NONE;
   p.SIDFilters       = fopt_sid_filters ? true : false;
   p.Emul1541Proc     = fopt_true_drive ? true : false;
   p.FastReset        = fopt_fast_reset ? true : false;
   p.SpriteCollisions = fopt_sprite_coll ? true : false;
   p.JoystickSwap     = (fopt_joy_port == 2);   /* swap => pad drives C64 port 2 */
   p.REUSize          = fopt_reu;

   if (TheC64)
      TheC64->NewPrefs(&p);   /* live: reconfigures SID/REU/drive/kernal as needed */
   ThePrefs = p;
}
