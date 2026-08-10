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
#include <sys/time.h> /* TODO/FIXME - get rid of gettimeofday */
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
	timeval tv;
	gettimeofday(&tv, NULL);
	srand(tv.tv_usec);

	if (!init_graphics())
		return 0;

	the_app = new Frodo();
	the_app->ArgvReceived(argc, argv);
	the_app->ReadyToRun();
#ifndef NO_LIBCO
	delete the_app;
#endif
	return 0;
}

#ifdef NO_LIBCO
void quit_frodo_emu(void)
{
	delete TheC64;
	delete the_app;
}
#endif

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
#if defined (__vita__) || defined(__psp__)
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
#endif
}

/* Determine whether path name refers to a directory */

bool IsDirectory(const char *path)
{
   return path_is_directory(path);
}
