/*
  Hatari - scandir.h

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.
*/
#ifndef HATARI_SCANDIR_H
#define HATARI_SCANDIR_H

#include <dirent.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(HAVE_ALPHASORT) && !HAVE_ALPHASORT
extern int alphasort(const struct dirent **d1, const struct dirent **d2);
#endif

#if defined(HAVE_SCANDIR) && !HAVE_SCANDIR
extern int scandir(const char *dirname, struct dirent ***namelist,
                   int (*sdfilter)(const struct dirent *),
                   int (*comp)(const struct dirent **, const struct dirent **));
#endif

#ifdef __cplusplus
}
#endif

#endif /* HATARI_SCANDIR_H */
