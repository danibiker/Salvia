/*
	modded for libretro-frodo
*/

/*
  Hatari - file.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.

  Common file access functions.
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <errno.h>
#include <string.h>
#include <zlib.h>
#include <file/file_path.h>
#include <streams/file_stream.h>

#include "utype.h"

#include "dialog.h"
#include "file.h"
#include "zip.h"

/* Forward declarations */
extern "C" {
RFILE* rfopen(const char *path, const char *mode);
int64_t rfseek(RFILE* stream, int64_t offset, int origin);
int64_t rftell(RFILE* stream);
int rfclose(RFILE* stream);
int64_t rfread(void* buffer,
   size_t elem_size, size_t elem_count, RFILE* stream);
}

/*-----------------------------------------------------------------------*/
/**
 * Remove any '/'s from end of filenames, but keeps / intact
 */
void File_CleanFileName(char *pszFileName)
{
	int len = strlen(pszFileName);
	/* Remove end slashes from filename! But / remains! Doh! */
	while (len > 2 && pszFileName[--len] == PATHSEP)
		pszFileName[len] = '\0';
}


/*-----------------------------------------------------------------------*/
/**
 * Add '/' to end of filename
 */
void File_AddSlashToEndFileName(char *pszFileName)
{
   int len = strlen(pszFileName);
   /* Check dir/filenames */
   if (len == 0)
      return;

   if (pszFileName[len-1] != PATHSEP)
   {
      pszFileName[len] = PATHSEP; /* Must use end slash */
      pszFileName[len+1] = '\0';
   }
}


/*-----------------------------------------------------------------------*/
/**
 * Does filename extension match? If so, return TRUE
 */
bool File_DoesFileExtensionMatch(const char *pszFileName, const char *pszExtension)
{
   if (strlen(pszFileName) < strlen(pszExtension))
      return false;
   /* Is matching extension? */
   if (!strcasecmp(&pszFileName[strlen(pszFileName)-strlen(pszExtension)], pszExtension))
      return true;
   /* No */
   return false;
}


/*-----------------------------------------------------------------------*/
/**
 * Check if filename is from root
 *
 * Return TRUE if filename is '/', else give FALSE
 */
static bool File_IsRootFileName(const char *pszFileName)
{
	if (pszFileName[0] == '\0')     /* If NULL string return! */
		return false;

	if (pszFileName[0] == PATHSEP)
		return true;

#ifdef _WIN32
	if (pszFileName[1] == ':')
		return true;
#endif

#ifdef GEKKO
	if (strlen(pszFileName) > 2 && pszFileName[2] == ':')	// sd:
		return true;
	if (strlen(pszFileName) > 3 && pszFileName[3] == ':')	// fat:
		return true;
	if (strlen(pszFileName) > 4 && pszFileName[4] == ':')	// fat3:
		return true;
#endif

	return false;
}


/*-----------------------------------------------------------------------*/
/**
 * Return string, to remove 'C:' part of filename
 */
const char *File_RemoveFileNameDrive(const char *pszFileName)
{
   if ( (pszFileName[0] != '\0') && (pszFileName[1] == ':') )
      return &pszFileName[2];
   return pszFileName;
}

/*-----------------------------------------------------------------------*/
/**
 * Check if filename end with a '/'
 *
 * Return TRUE if filename ends with '/'
 */
bool File_DoesFileNameEndWithSlash(char *pszFileName)
{
	if (pszFileName[0] == '\0')    /* If NULL string return! */
		return false;

	/* Does string end in a '/'? */
	if (pszFileName[strlen(pszFileName)-1] == PATHSEP)
		return true;

	return false;
}


/*-----------------------------------------------------------------------*/
/**
 * Read file from disk into allocated buffer and return the buffer
 * or NULL for error.  If pFileSize is non-NULL, read file size
 * is set to that.
 */
Uint8 *File_Read(const char *pszFileName, long *pFileSize, const char * const ppszExts[])
{
	char *filepath = NULL;
	Uint8 *pFile   = NULL;
	long FileSize  = 0;

	/* Does the file exist? If not, see if it can scan 
      for other extensions and try these */
	if (!path_is_valid(pszFileName) && ppszExts)
	{
		/* Try other extensions, if succeeds, returns correct one */
		filepath = File_FindPossibleExtFileName(pszFileName, ppszExts);
	}
	if (!filepath)
		filepath = strdup(pszFileName);

	/* Is it a gzipped file? */
	if (File_DoesFileExtensionMatch(filepath, ".gz"))
	{
		/* Open and read gzipped file */
		gzFile hGzFile = gzopen(filepath, "rb");
		if (hGzFile)
		{
			/* Find size of file: */
			do
			{
				/* Seek through the file until we hit the end... */
				char tmp[1024];
				if (gzread(hGzFile, tmp, sizeof(tmp)) < 0)
					return NULL;
			}
			while (!gzeof(hGzFile));
			FileSize = gztell(hGzFile);
			gzrewind(hGzFile);
			/* Read in... */
			pFile = (Uint8*)malloc(FileSize);
			if (pFile)
				FileSize = gzread(hGzFile, pFile, FileSize);

			gzclose(hGzFile);
		}
	}
	else if (File_DoesFileExtensionMatch(filepath, ".zip"))
	{
		/* It is a .ZIP file! -> Try to load the first file in the archive */
		pFile = ZIP_ReadFirstFile(filepath, &FileSize, ppszExts);
	}
	else          /* It is a normal file */
	{
		/* Open and read normal file */
		RFILE *hDiskFile = rfopen(filepath, "rb");
		if (hDiskFile)
		{
			/* Find size of file: */
			rfseek(hDiskFile, 0, SEEK_END);
			FileSize = rftell(hDiskFile);
			rfseek(hDiskFile, 0, SEEK_SET);
			/* Read in... */
			pFile = (Uint8*)malloc(FileSize);
			if (pFile)
				FileSize = rfread(pFile, 1, FileSize, hDiskFile);
			rfclose(hDiskFile);
		}
	}
	free(filepath);

	/* Store size of file we read in (or 0 if failed) */
	if (pFileSize)
		*pFileSize = FileSize;

	return pFile;        /* Return to where read in/allocated */
}

/*-----------------------------------------------------------------------*/
/**
 * Try filename with various extensions and check if file exists
 * - if so, return allocated string which caller should free,
 *   otherwise return NULL
 */
char * File_FindPossibleExtFileName(
      const char *pszFileName, const char * const ppszExts[])
{
	int i;
	char *szSrcName, *szSrcExt;
	/* Allocate temporary memory for strings: */
	char *szSrcDir = (char*)malloc(3 * FILENAME_MAX);
	if (!szSrcDir)
		return NULL;
	szSrcName = szSrcDir + FILENAME_MAX;
	szSrcExt  = szSrcName + FILENAME_MAX;

	/* Split filename into parts */
	File_SplitPath(pszFileName, szSrcDir, szSrcName, szSrcExt);

	/* Scan possible extensions */
	for (i = 0; ppszExts[i]; i++)
	{
		char *szTempFileName;

		/* Re-build with new file extension */
		szTempFileName = File_MakePath(szSrcDir, szSrcName, ppszExts[i]);
		if (szTempFileName)
		{
			/* Does this file exist? */
			if (path_is_valid(szTempFileName))
			{
				free(szSrcDir);
				/* return filename without extra strings */
				return szTempFileName;
			}
			free(szTempFileName);
		}
	}
	free(szSrcDir);
	return NULL;
}


/*-----------------------------------------------------------------------*/
/**
 * Split a complete filename into path, filename and extension.
 * If pExt is NULL, don't split the extension from the file name!
 * It's safe for pSrcFileName and pDir to be the same string.
 */
void File_SplitPath(const char *pSrcFileName, char *pDir, char *pName, char *pExt)
{
	char *ptr2;
	/* Build pathname: */
	char *ptr1 = (char *)strrchr(pSrcFileName, PATHSEP);
	if (ptr1)
	{
		strcpy(pName, ptr1+1);
		memmove(pDir, pSrcFileName, ptr1-pSrcFileName);
		pDir[ptr1-pSrcFileName] = 0;
	}
	else
	{
 		strcpy(pName, pSrcFileName);
		sprintf(pDir, ".%c", PATHSEP);
	}

	/* Build the raw filename: */
	if (pExt)
	{
		ptr2 = strrchr(pName+1, '.');
		if (ptr2)
		{
			pName[ptr2-pName] = 0;
			/* Copy the file extension: */
			strcpy(pExt, ptr2+1);
		}
		else
			pExt[0] = 0;
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Construct a complete filename from path, filename and extension.
 * Return the constructed filename.
 * pExt can also be NULL.
 */
char * File_MakePath(const char *pDir, const char *pName, const char *pExt)
{
	/* dir or "." + "/" + name + "." + ext + \0 */
	int len        = strlen(pDir) + 2 + strlen(pName) + 1 + (pExt ? strlen(pExt) : 0) + 1;
	char *filepath = (char*)malloc(len);
	if (!filepath)
		return NULL;
	if (!pDir[0])
	{
		filepath[0] = '.';
		filepath[1] = '\0';
	}
   else
		strcpy(filepath, pDir);
	len = strlen(filepath);
	if (filepath[len-1] != PATHSEP)
		filepath[len++] = PATHSEP;
	strcpy(&filepath[len], pName);

	if (pExt != NULL && pExt[0])
	{
		len += strlen(pName);
		if (pExt[0] != '.')
			strcat(&filepath[len++], ".");
		strcat(&filepath[len], pExt);
	}
	return filepath;
}


/*-----------------------------------------------------------------------*/
/**
 * Shrink a file name to a certain length and insert some dots if we cut
 * something away (useful for showing file names in a dialog).
 */
void File_ShrinkName(char *pDestFileName, const char *pSrcFileName, int maxlen)
{
	int srclen = strlen(pSrcFileName);
	if (srclen < maxlen)
		strcpy(pDestFileName, pSrcFileName);  /* It fits! */
	else
	{
		strncpy(pDestFileName, pSrcFileName, maxlen/2);
		if (maxlen&1)  /* even or uneven? */
			pDestFileName[maxlen/2-1] = 0;
		else
			pDestFileName[maxlen/2-2] = 0;
		strcat(pDestFileName, "...");
		strcat(pDestFileName, &pSrcFileName[strlen(pSrcFileName)-maxlen/2+1]);
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Create a clean absolute file name from a (possibly) relative file name.
 * I.e. filter out all occurancies of "./" and "../".
 * pFileName needs to point to a buffer of at least FILENAME_MAX bytes.
 */
void File_MakeAbsoluteName(char *pFileName)
{
   int inpos = 0;
   int outpos = 0;
   char *pTempName = (char*)malloc(FILENAME_MAX);
   if (!pTempName)
      return;

   /* Is it already an absolute name? */
#if !defined(__psp__) && !defined(__vita__)
   if (File_IsRootFileName(pFileName))
      outpos = 0;
   else
   {
      if (!getcwd(pTempName, FILENAME_MAX))
      {
         free(pTempName);
         return;
      }
      File_AddSlashToEndFileName(pTempName);
      outpos = strlen(pTempName);
   }
#endif

   /* Now filter out the relative paths "./" and "../" */
   while (pFileName[inpos] != 0 && outpos < FILENAME_MAX)
   {
      if (pFileName[inpos] == '.' && pFileName[inpos+1] == PATHSEP)
      {
         /* Ignore "./" */
         inpos += 2;
      }
      else if (pFileName[inpos] == '.' && pFileName[inpos+1] == 0)
      {
         inpos += 1;        /* Ignore "." at the end of the path string */
         if (outpos > 1)
            pTempName[outpos - 1] = 0;   /* Remove the last slash, too */
      }
      else if (pFileName[inpos] == '.' && pFileName[inpos+1] == '.'
            && (pFileName[inpos+2] == PATHSEP || pFileName[inpos+2] == 0))
      {
         /* Handle "../" */
         char *pSlashPos;
         inpos += 2;
         pTempName[outpos - 1] = 0;
         pSlashPos = strrchr(pTempName, PATHSEP);
         if (pSlashPos)
         {
            *(pSlashPos + 1) = 0;
            outpos = strlen(pTempName);
         }
         else
         {
            pTempName[0] = PATHSEP;
            outpos = 1;
         }
         /* Were we already at the end of the string or is there more to come? */
         if (pFileName[inpos] == PATHSEP)
         {
            /* There was a slash after the '..', so skip slash and
             * simply proceed with next part */
            inpos += 1;
         }
         else
         {
            /* We were at the end of the string, so let's remove the slash
             * from the new string, too */
            if (outpos > 1)
               pTempName[outpos - 1] = 0;
         }
      }
      else
      {
         /* Copy until next slash or end of input string */
         while (pFileName[inpos] != 0 && outpos < FILENAME_MAX)
         {
            pTempName[outpos++] = pFileName[inpos++];
            if (pFileName[inpos - 1] == PATHSEP)
               break;
         }
      }
   }

   pTempName[outpos] = 0;

   strcpy(pFileName, pTempName);          /* Copy back */
   free(pTempName);
}


/*-----------------------------------------------------------------------*/
/**
 * Create a valid path name from a possibly invalid name by erasing invalid
 * path parts at the end of the string.  If string doesn't contain any path
 * component, it will be pointed to the root directory.  Empty string will
 * be left as-is to prevent overwriting past allocated area.
 */
void File_MakeValidPathName(char *pPathName)
{
	struct stat dirstat;
	char *pLastSlash;

	do
	{
		/* Check for a valid path */
		if (path_is_directory(pPathName))
			break;

		pLastSlash = strrchr(pPathName, PATHSEP);
		if (pLastSlash) /* Erase (probably invalid) part after last slash */
			*pLastSlash = 0;
		else
		{
			if (pPathName[0])
			{
				/* point to root */
				pPathName[0] = PATHSEP;
				pPathName[1] = 0;
			}
			return;
		}
	}
	while (pLastSlash);

	/* Make sure that path name ends with a slash */
	File_AddSlashToEndFileName(pPathName);
}


/*-----------------------------------------------------------------------*/
/**
 * Remove given number of path elements from the end of the given path.
 * Leaves '/' at the end if path still has directories. Given path
 * may not be empty.
 */
void File_PathShorten(char *path, int dirs)
{
	int n = 0;
	/* ignore last char, it may or may not be '/' */
	int i = strlen(path)-1;
	while(i > 0 && n < dirs)
   {
      if (path[--i] == PATHSEP)
         n++;
   }
	if (path[i] == PATHSEP)
		path[i+1] = '\0';
	else
   {
      path[0] = PATHSEP;
      path[1] = '\0';
   }
}


/*-----------------------------------------------------------------------*/
/*
  If "/." or "/.." at end, remove that and in case of ".." remove
  also preceding dir (go one dir up).  Leave '/' at the end of
  the path.
*/
void File_HandleDotDirs(char *path)
{
	int len = strlen(path);
	if (   len         >= 2
	    && path[len-2] == PATHSEP
	    && path[len-1] == '.') /* keep in same dir */
		path[len-1] = '\0';
	else if (
         len          >= 3
	    && path[len-3] == PATHSEP
	    && path[len-2] == '.'
	    && path[len-1] == '.')
	{
		/* go one dir up */
		if (len == 3)
			path[1] = 0;		/* already root */
		else
      {
         char *ptr;
         path[len-3] = 0;
         ptr = strrchr(path, PATHSEP);
         if (ptr)
            *(ptr+1) = 0;
      }
	}
}
