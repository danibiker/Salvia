/* 
 * Copyright (c) 2003 NONAKA Kimihiro
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgment:
 *      This product includes software developed by NONAKA Kimihiro.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <strings.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#ifdef USE_LIBRETRO_VFS
#include "dosio.h"

static int _WritePrivateProfileString_subr(
        FILEH **fp, int64_t pos, int64_t nowpos,
        const char *buf, const char *file)
{
	char *p         = NULL;
	int32_t st_size = 0;

	if (wrap_vfs_stat(file, &st_size) == 0)
		return 1;

	if (!(p = (char *)malloc(st_size)))
		return 1;

	file_rewind(*fp);
	if (file_read(*fp, p, st_size) < 1)
		goto out;
	file_close(*fp);

	*fp = file_create(file);
	if (*fp == NULL)
		goto out;
	if (file_write(*fp, p, pos) < 1)
		goto out;
	if (file_write(*fp, (char *)buf, strlen(buf)) < 1)
		goto out;
	if (((int64_t)st_size - nowpos) > 0)
	{
		if (file_write(*fp, p + nowpos, st_size - nowpos) < 1)
			goto out;
	}
	free(p);
	return 0;

out:
	free(p);
	return 1;
}

int WritePrivateProfileString(
        const char *sect, const char *key,
        const char *str, const char *inifile)
{
	char lbuf[256];
	char newbuf[256];

	FILEH *fp     = NULL;
	int64_t pos   = 0;
	int found    = 0;
	int notfound = 0;
	int delta    = 0;

	if (wrap_vfs_stat(inifile, 0) == 1)
		fp = file_open(inifile);
	else
		fp = file_create(inifile);

	if (!fp)
		return 0;

	while (!file_eof(fp))
	{
		file_gets(fp, lbuf, sizeof(lbuf));
		/* XXX should be case insensitive */
		if (lbuf[0] == '['
			&& !strncasecmp(sect, &lbuf[1], strlen(sect))
			&& lbuf[strlen(sect) + 1] == ']')
		{
			found = 1;
			break;
		}
	}
	if (file_eof(fp) && !found)
	{
		/*
		 * Now create new section and key.
		 */
		file_rewind(fp);
		sprintf(newbuf, "[%s]\n", sect);
		if (file_write(fp, newbuf, strlen(newbuf)) < 1)
			goto writefail;
		sprintf(newbuf, "%s=%s\n", key, str);
		if (file_write(fp, newbuf, strlen(newbuf)) < 1)
			goto writefail;
		file_close(fp);
		return 1;
	}

	pos   = 0; /* gcc happy */
	found = 0;
	while (!file_eof(fp))
	{
		pos = file_tell(fp);
		file_gets(fp, lbuf, sizeof(lbuf));
		if (lbuf[0] == '[' && strchr(lbuf, ']'))
		{
			notfound = 1;
			break;
		}
		/* XXX should be case insensitive */
		if (!strncasecmp(key, lbuf, strlen(key))
			&& lbuf[strlen(key)] == '=')
		{
			found = 1;
			sprintf(newbuf, "%s=%s\n", key, str);
			delta = strlen(newbuf) - strlen(lbuf);
			if (delta == 0)
			{
				if (!strncasecmp(newbuf, lbuf, strlen(newbuf)))
					break;
				/* overwrite */
				file_seek(fp, pos, FSEEK_SET);
				if (file_write(fp, newbuf, strlen(newbuf)) < 1)
					goto writefail;
			}
			else if (delta > 0)
			{
				if (!_WritePrivateProfileString_subr(&fp, pos,
					file_tell(fp), newbuf, inifile))
					goto writefail;
			}
			else
			{
				if (!_WritePrivateProfileString_subr(&fp, pos,
					file_tell(fp), newbuf, inifile))
					goto writefail;
			}
			break;
		}
	}
	if (file_eof(fp) && !found)
	{
		/* Now create new key. */
		file_seek(fp, 0L, FSEEK_END);
		sprintf(newbuf, "%s=%s\n", key, str);
		if (file_write(fp, newbuf, strlen(newbuf)) < 1)
			goto writefail;
	}
	else if (notfound)
	{
		sprintf(newbuf, "%s=%s\n", key, str);
		if (!_WritePrivateProfileString_subr(&fp, pos,
			file_tell(fp), newbuf, inifile))
			goto writefail;
	}

	file_close(fp);
	return 1;

writefail:
	file_close(fp);
	return 0;
}

#else /* !USE_LIBRETRO_VFS */

static int
_WritePrivateProfileString_subr(FILE **fp, long pos, long nowpos,
		const char *buf, const char *file)
{
	struct stat sb;
	char *p;

	if (stat(file, &sb) < 0)
		return 1;

	if (!(p = (char *)malloc(sb.st_size)))
		return 1;
	rewind(*fp);
	if (fread(p, sb.st_size, 1, *fp) < 1)
		goto out;
	fclose(*fp);

	*fp = fopen(file, "w+");
	if (*fp == NULL)
		goto out;
	if (fwrite(p, pos, 1, *fp) < 1)
		goto out;
	if (fwrite(buf, strlen(buf), 1, *fp) < 1)
		goto out;
	if (sb.st_size - nowpos > 0)
		if (fwrite(p + nowpos, sb.st_size - nowpos, 1, *fp) < 1)
			goto out;
	free(p);
	return 0;

out:
	free(p);
	return 1;
}

int WritePrivateProfileString(const char *sect, const char *key,
		const char *str, const char *inifile)
{
	char lbuf[256];
	char newbuf[256];
	struct stat sb;
	FILE *fp;
	long pos;
	int found = 0;
	int notfound = 0;

	if (stat(inifile, &sb) == 0)
		fp = fopen(inifile, "r+");
	else
		fp = fopen(inifile, "w+");
	if (!fp)
		return 0;

	while (!feof(fp))
	{
		fgets(lbuf, sizeof(lbuf), fp);
		/* XXX should be case insensitive */
		if (lbuf[0] == '['
		    && !strncasecmp(sect, &lbuf[1], strlen(sect))
		    && lbuf[strlen(sect) + 1] == ']')
		{
			found = 1;
			break;
		}
	}
	if (feof(fp) && !found)
	{
		/* Now create new section and key. */
		rewind(fp);
		snprintf(newbuf, sizeof(newbuf), "[%s]\n", sect);
		if (fwrite(newbuf, strlen(newbuf), 1, fp) < 1)
			goto writefail;
		snprintf(newbuf, sizeof(newbuf), "%s=%s\n", key, str);
		if (fwrite(newbuf, strlen(newbuf), 1, fp) < 1) 
			goto writefail;
		fclose(fp);
		return 1;
	}

	pos   = 0;	/* gcc happy */
	found = 0;
	while (!feof(fp))
	{
		pos = ftell(fp);
		fgets(lbuf, sizeof(lbuf), fp);
		if (lbuf[0] == '[' && strchr(lbuf, ']'))
		{
			notfound = 1;
			break;
		}
		/* XXX should be case insensitive */
		if (!strncasecmp(key, lbuf, strlen(key))
		    && lbuf[strlen(key)] == '=')
		{
			size_t delta;
			found = 1;
			snprintf(newbuf, sizeof(newbuf), "%s=%s\n", key, str);
			delta = strlen(newbuf) - strlen(lbuf);
			if (delta == 0)
			{
				if (!strncasecmp(newbuf, lbuf, strlen(newbuf)))
					break;
				/* overwrite */
				fseek(fp, pos, SEEK_SET);
				if (fwrite(newbuf, strlen(newbuf), 1, fp) < 1)
					goto writefail;
			}
			else if (delta > 0)
			{
				if (!_WritePrivateProfileString_subr(&fp, pos,
				    ftell(fp), newbuf, inifile))
					goto writefail;
			}
			else
			{
				if (!_WritePrivateProfileString_subr(&fp, pos,
				    ftell(fp), newbuf, inifile))
					goto writefail;
			}
			break;
		}
	}
	if (feof(fp) && !found)
	{
		/* Now create new key. */
		fseek(fp, 0L, SEEK_END);
		snprintf(newbuf, sizeof(newbuf), "%s=%s\n", key, str);
		if (fwrite(newbuf, strlen(newbuf), 1, fp) < 1)
			goto writefail;
	}
	else if (notfound)
	{
		snprintf(newbuf, sizeof(newbuf), "%s=%s\n", key, str);
		if (!_WritePrivateProfileString_subr(&fp, pos,
		    ftell(fp), newbuf, inifile))
			goto writefail;
	}

	fclose(fp);
	return 1;

writefail:
	fclose(fp);
	return 0;
}
#endif