/*
	modded for libretro-uae
*/

/*
  Hatari - zip.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.

  Zipped disk support, uses zlib
*/

#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <string.h>
#include <zlib.h>

#include "file.h"
#include "unzip.h"
#include "zip.h"

#ifdef QNX
#include <sys/dir.h>
#define dirent direct
#endif

/* #define SAVE_TO_ZIP_IMAGES */

#define ZIP_PATH_MAX  256

#define ZIP_FILE_D64  1
#define ZIP_FILE_T64  2
#define ZIP_FILE_C64  3


/* Possible disk image extensions to scan for */
static const char * const pszDiskNameExts[] =
{
  ".d64",
  ".t64",
  ".c64",
  NULL
};


/*-----------------------------------------------------------------------*/
/**
 * Does filename end with a .ZIP extension? If so, return true.
 */
bool ZIP_FileNameIsZIP(const char *pszFileName)
{
	return File_DoesFileExtensionMatch(pszFileName,".zip");
}


/*-----------------------------------------------------------------------*/
/**
 * Check if a file name contains a slash or backslash and return its position.
 */
static int Zip_FileNameHasSlash(const char *fn)
{
	int i=0;

	while (fn[i] != '\0')
	{
		if (fn[i] == '\\' || fn[i] == '/')
			return i;
		i++;
	}
	return -1;
}


/*-----------------------------------------------------------------------*/
/**
 * Returns a list of files from a zip file. returns NULL on failure,
 * returns a pointer to an array of strings if successful. Sets nfiles
 * to the number of files.
 */
zip_dir *ZIP_GetFiles(const char *pszFileName)
{
	int nfiles;
	unsigned int i;
	unz_global_info gi;
	int err;
	unzFile uf;
	char **filelist;
	unz_file_info file_info;
	char filename_inzip[ZIP_PATH_MAX];
	zip_dir *zd;

	if (!(uf = unzOpen(pszFileName)))
		return NULL;

	if ((err = unzGetGlobalInfo(uf,&gi)) != UNZ_OK)
		return NULL;

	/* allocate a file list */
	filelist = (char **)malloc(gi.number_entry*sizeof(char *));
	if (!filelist)
		return NULL;

	nfiles = gi.number_entry;  /* set the number of files */

	for (i = 0; i < gi.number_entry; i++)
	{
		err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, ZIP_PATH_MAX, NULL, 0, NULL, 0);
		if (err != UNZ_OK)
		{
			free(filelist);
			return NULL;
		}

		filelist[i] = (char *)malloc(strlen(filename_inzip) + 1);
		if (!filelist[i])
		{
			free(filelist);
			return NULL;
		}

		strcpy(filelist[i], filename_inzip);
		if ((i+1) < gi.number_entry)
		{
			err = unzGoToNextFile(uf);
			if (err != UNZ_OK)
			{
				/* deallocate memory */
				for (; i > 0; i--)
					free(filelist[i]);
				free(filelist);
				return NULL;
			}
		}
	}

	unzClose(uf);

	zd = (zip_dir *)malloc(sizeof(zip_dir));
	if (!zd)
	{
		free(filelist);
		return NULL;
	}
	zd->names = filelist;
	zd->nfiles = nfiles;

	return zd;
}


/*-----------------------------------------------------------------------*/
/**
 * Free the memory that has been allocated for a zip_dir.
 */
void ZIP_FreeZipDir(zip_dir *f_zd)
{
	while (f_zd->nfiles > 0)
	{
		f_zd->nfiles--;
		free(f_zd->names[f_zd->nfiles]);
		f_zd->names[f_zd->nfiles] = NULL;
	}
	free(f_zd->names);
	f_zd->names = NULL;
	free(f_zd);
}


/*-----------------------------------------------------------------------*/
/**
 * Free the memory that has been allocated for fentries.
 */
static void ZIP_FreeFentries(struct dirent **fentries, int entries)
{
	while (entries > 0)
	{
		entries--;
		free(fentries[entries]);
	}
	free(fentries);
}


/*-----------------------------------------------------------------------*/
/**
 *   Returns a list of files from the directory (dir) in a zip file list (zip)
 *   sets entries to the number of entries and returns a dirent structure, or
 *   NULL on failure. NOTE: only f_name is set in the dirent structures. 
 */
struct dirent **ZIP_GetFilesDir(const zip_dir *zip, const char *dir, int *entries)
{
   int i,j;
   char *temp;
   bool flag;
   int slash;
   struct dirent **fentries;
   zip_dir *files = (zip_dir *)malloc(sizeof(zip_dir));
   if (!files)
      return NULL;

   files->names = (char **)malloc((zip->nfiles + 1) * sizeof(char *));
   if (!files->names)
   {
      free(files);
      return NULL;
   }

   /* add ".." directory */
   files->nfiles = 1;
   temp = (char *)malloc(4);
   if (!temp)
   {
      ZIP_FreeZipDir(files);
      return NULL;
   }
   temp[0] = temp[1] = '.';
   temp[2] = '/';
   temp[3] = '\0';
   files->names[0] = temp;

   for (i = 0; i < zip->nfiles; i++)
   {
      if (strlen(zip->names[i]) > strlen(dir))
      {
         if (strncasecmp(zip->names[i], dir, strlen(dir)) == 0)
         {
            temp = zip->names[i];
            temp = (char *)(temp + strlen(dir));
            if (temp[0] != '\0')
            {
               if ((slash=Zip_FileNameHasSlash(temp)) > 0)
               {
                  /* file is in a subdirectory, add this subdirectory if it doesn't exist in the list */
                  flag = false;
                  for (j = files->nfiles-1; j > 0; j--)
                  {
                     if (strncasecmp(temp, files->names[j], slash+1) == 0)
                        flag = true;
                  }
                  if (flag == false)
                  {
                     files->names[files->nfiles] = (char *)malloc(slash+2);
                     if (!files->names[files->nfiles])
                     {
                        ZIP_FreeZipDir(files);
                        return NULL;
                     }
                     strncpy(files->names[files->nfiles], temp, slash+1);
                     ((char *)files->names[files->nfiles])[slash+1] = '\0';
                     files->nfiles++;
                  }
               }
               else
               {
                  /* add a filename */
                  files->names[files->nfiles] = (char *)malloc(strlen(temp)+1);
                  if (!files->names[files->nfiles])
                  {
                     ZIP_FreeZipDir(files);
                     return NULL;
                  }
                  strncpy(files->names[files->nfiles], temp, strlen(temp));
                  ((char *)files->names[files->nfiles])[strlen(temp)] = '\0';
                  files->nfiles++;
               }
            }
         }
      }
   }

   /* copy to a dirent structure */
   *entries = files->nfiles;
   fentries = (struct dirent **)malloc(sizeof(struct dirent *)*files->nfiles);
   if (!fentries)
   {
      ZIP_FreeZipDir(files);
      return NULL;
   }
   for (i = 0; i < files->nfiles; i++)
   {
      fentries[i] = (struct dirent *)malloc(sizeof(struct dirent));
      if (!fentries[i])
      {
         ZIP_FreeFentries(fentries, i+1);
         return NULL;
      }
      strcpy(fentries[i]->d_name, files->names[i]);
   }

   ZIP_FreeZipDir(files);

   return fentries;
}


/*-----------------------------------------------------------------------*/
/**
 * Check an image file in the archive, return the uncompressed length
 */
static long ZIP_CheckImageFile(unzFile uf, char *filename, int namelen, int *pDiskType)
{
	unz_file_info file_info;

	if (unzLocateFile(uf,filename, 0) != UNZ_OK)
		return -1;

	if (unzGetCurrentFileInfo(uf, &file_info, filename, namelen, NULL, 0, NULL, 0) != UNZ_OK)
		return -1;

	/* check for a .msa or .st extension */
	
/*
	if (MSA_FileNameIsMSA(filename, false))
	{
		*pDiskType = ZIP_FILE_MSA;
		return file_info.uncompressed_size;
	}

	if (ST_FileNameIsST(filename, false))
	{
		*pDiskType = ZIP_FILE_ST;
		return file_info.uncompressed_size;
	}

	if (DIM_FileNameIsDIM(filename, false))
	{
		*pDiskType = ZIP_FILE_DIM;
		return file_info.uncompressed_size;
	}
	
*/
	*pDiskType = -1;
	return file_info.uncompressed_size;
}

/*-----------------------------------------------------------------------*/
/**
 * Return the first matching file in a zip, or NULL on failure.
 * String buffer size is ZIP_PATH_MAX
 */
static char *ZIP_FirstFile(const char *filename, const char * const ppsExts[])
{
	zip_dir *files;
	int i, j;
	char *name;

	if (!(files = ZIP_GetFiles(filename)))
		return NULL;

	name = (char *)malloc(ZIP_PATH_MAX);
	if (!name)
		return NULL;

	/* Do we have to scan for a certain extension? */
	if (ppsExts)
	{
		name[0] = '\0';
		for(i = files->nfiles-1; i >= 0; i--)
		{
			for (j = 0; ppsExts[j] != NULL; j++)
			{
				if (File_DoesFileExtensionMatch(files->names[i], ppsExts[j]))
				{
					strncpy(name, files->names[i], ZIP_PATH_MAX);
					break;
				}
			}
		}
	}
	else
	{
		/* There was no extension given -> use the very first name */
		strncpy(name, files->names[0], ZIP_PATH_MAX);
	}

	/* free the files */
	ZIP_FreeZipDir(files);

	if (name[0] == '\0')
		return NULL;
	return name;
}


/*-----------------------------------------------------------------------*/
/**
 * Extract a file (filename) from a ZIP-file (uf), the number of 
 * bytes to uncompress is size. Returns a pointer to a buffer containing
 * the uncompressed data, or NULL.
 */
static void *ZIP_ExtractFile(unzFile uf, const char *filename, uLong size)
{
	int err = UNZ_OK;
	char filename_inzip[ZIP_PATH_MAX];
	void* buf;
	uInt size_buf;
	unz_file_info file_info;

	if (unzLocateFile(uf,filename, 0) != UNZ_OK)
		return NULL;

	err = unzGetCurrentFileInfo(uf,&file_info,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);

	if (err != UNZ_OK)
		return NULL;

	size_buf = size;
	buf = (char *)malloc(size_buf);
	if (!buf)
		return NULL;

	err = unzOpenCurrentFile(uf);
	if (err != UNZ_OK)
	{
		free(buf);
		return NULL;
	}

	do
	{
		err = unzReadCurrentFile(uf,buf,size_buf);
		if (err < 0)
			return NULL;
	}
	while (err > 0);

	return buf;
}


/*-----------------------------------------------------------------------*/
/**
 * Load disk image from a .ZIP archive into memory, set  the number
 * of bytes loaded into pImageSize and return the data or NULL on error.
 */
Uint8 *ZIP_ReadDisk(const char *pszFileName, const char *pszZipPath, long *pImageSize)
{
	uLong ImageSize=0;
	unzFile uf=NULL;
	Uint8 *buf;
	char *path;
	int nDiskType = -1;
	Uint8 *pDiskBuffer = NULL;

	*pImageSize = 0;

	if (!(uf = unzOpen(pszFileName)))
		return NULL;

	if (!pszZipPath || pszZipPath[0] == 0)
	{
		if (!(path = ZIP_FirstFile(pszFileName, pszDiskNameExts)))
		{
			unzClose(uf);
			return NULL;
		}
	}
	else
	{
		if (!(path = (char *)malloc(ZIP_PATH_MAX)))
		{
			unzClose(uf);
			return NULL;
		}
		strncpy(path, pszZipPath, ZIP_PATH_MAX);
		path[ZIP_PATH_MAX-1] = '\0';
	}

	ImageSize = ZIP_CheckImageFile(uf, path, ZIP_PATH_MAX, &nDiskType);
	if (ImageSize <= 0)
	{
		unzClose(uf);
		free(path);
		return NULL;
	}

	/* extract to buf */
	buf = (Uint8 *)ZIP_ExtractFile(uf, path, ImageSize);

	unzCloseCurrentFile(uf);
	unzClose(uf);
	free(path);
	path = NULL;

	if (!buf)
		return NULL;  /* failed extraction, return error */

	switch(nDiskType)
   {
      default:
         //	case ZIP_FILE_ST:
         /* ST image => return buffer directly */
         pDiskBuffer = buf;
         break;
   }
	
	if (pDiskBuffer)
		*pImageSize = ImageSize;
	return pDiskBuffer;
}


/*-----------------------------------------------------------------------*/
/**
 * Save .ZIP file from memory buffer. Returns true if all is OK.
 *
 * Not yet implemented.
 */
bool ZIP_WriteDisk(const char *pszFileName,unsigned char *pBuffer,int ImageSize)
{
	return false;
}

/*-----------------------------------------------------------------------*/
/**
 * Load first file from a .ZIP archive into memory, and return the number
 * of bytes loaded.
 */
Uint8 *ZIP_ReadFirstFile(const char *pszFileName, long *pImageSize, const char * const ppszExts[])
{
	unzFile uf=NULL;
	Uint8 *pBuffer;
	char *pszZipPath;
	unz_file_info file_info;

	*pImageSize = 0;

	/* Open the ZIP file */
	if (!(uf = unzOpen(pszFileName)))
		return NULL;

	/* Locate the first file in the ZIP archive */
	pszZipPath = ZIP_FirstFile(pszFileName, ppszExts);
	if (!pszZipPath)
	{
		unzClose(uf);
		return NULL;
	}

	if (unzLocateFile(uf, pszZipPath, 0) != UNZ_OK)
      goto error;

	/* Get file information (file size!) */
	if (unzGetCurrentFileInfo(uf, &file_info,
            pszZipPath, ZIP_PATH_MAX, NULL, 0, NULL, 0) != UNZ_OK)
      goto error;

	/* Extract to buffer */
	pBuffer = (Uint8 *)ZIP_ExtractFile(uf, pszZipPath, file_info.uncompressed_size);

	/* And close the file */
	unzCloseCurrentFile(uf);
	unzClose(uf);

	free(pszZipPath);

	if (pBuffer)
		*pImageSize = file_info.uncompressed_size;

	return pBuffer;

error:
   free(pszZipPath);
   return NULL;
}
