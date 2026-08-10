/*
	modded for libretro-frodo
*/

/*
  Hatari - paths.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.

  Set up the various path strings.
*/

#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>

#include "file.h"

#include "paths.h"

#if defined(VITA)
#  include <psp2/io/fcntl.h>
#  include <psp2/io/dirent.h>
#  include <psp2/io/stat.h>
#elif defined(PSP)
#  include <pspiofilemgr.h>
#endif

static char sWorkingDir[FILENAME_MAX];    /* Working directory */
static char sUserHomeDir[FILENAME_MAX];   /* User's home directory ($HOME) */

/**
 * Return pointer to current working directory string
 */
const char *Paths_GetWorkingDir(void)
{
	return sWorkingDir;
}

/**
 * Return pointer to user's home directory string
 */
const char *Paths_GetUserHome(void)
{
	return sUserHomeDir;
}

/**
 * Initialize the users home directory string
 * and Hatari's home directory (~/.hatari)
 */
static void Paths_InitHomeDirs(void)
{
   char *psHome = getenv("HOME");
   if (psHome)
      strncpy(sUserHomeDir, psHome, FILENAME_MAX);
#if defined(WIN32)
   else
   {
      char *psDrive;
      int len = 0;
      /* Windows home path? */
      psHome = getenv("HOMEPATH");
      psDrive = getenv("HOMEDRIVE");
      if (psDrive)
      {
         len = strlen(psDrive);
         len = len < FILENAME_MAX ? len : FILENAME_MAX;
         strncpy(sUserHomeDir, psDrive, len);
      }
      if (psHome)
         strncpy(sUserHomeDir+len, psHome, FILENAME_MAX-len);
   }
#endif
   /* $HOME not set, so let's use current working dir as home */
   if (!psHome)
      strcpy(sUserHomeDir, sWorkingDir);
   else
      sUserHomeDir[FILENAME_MAX-1] = 0;
}

/**
 * Initialize directory names
 *
 * The datadir will be initialized relative to the bindir (where the executable
 * has been installed to). This means a lot of additional effort since we first
 * have to find out where the executable is. But thanks to this effort, we get
 * a relocatable package (we don't have any absolute path names in the program)!
 */
void Paths_Init(const char *argv0)
{
   /* Init working directory string */
#ifdef VITA
   strcpy(sWorkingDir, "ux0:/");
#elif defined(PSP)
   strcpy(sWorkingDir, "ms0:/");
#else
   if (getcwd(sWorkingDir, FILENAME_MAX) == NULL)
   {
      /* This should never happen... just in case... */
      strcpy(sWorkingDir, ".");
   }
#endif

   /* Init the user's home directory string */
   Paths_InitHomeDirs();
}
