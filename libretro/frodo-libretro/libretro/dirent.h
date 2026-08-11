/*
 * Minimal C89 dirent shim for the VS2010 (C89) Windows build of this core.
 *
 * Salvia already ships a fuller dirent compat at src/compat/dirent.h, but that
 * file uses C99 constructs (declarations after statements, `for (size_t i…)`)
 * and is only ever included from C++ translation units in the frontend. This
 * core's .c files compile as C89 under MSVC/VS2010, so that header cannot be
 * included verbatim here. This is a compact, C89-clean equivalent using the
 * same Win32 FindFirstFile approach, exposing just the pieces SYSROM_FindInDir()
 * needs: DIR, struct dirent (d_name only), opendir(), readdir(), closedir().
 *
 * Resolved via <dirent.h> because $(ProjectDir)\libretro is on the include path
 * and MSVC has no system dirent.h. Enabled by HAVE_DIRENT_H / HAVE_OPENDIR
 * (see libretro/config.h).
 */
#ifndef ATARI800_LIBRETRO_DIRENT_H
#define ATARI800_LIBRETRO_DIRENT_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* MSVC's <sys/stat.h> ships the _S_IF* constants but not the POSIX S_IF* /
   S_ISxxx names. devices.c and ui_basic.c use S_ISDIR() (unguarded, expecting
   a real dirent.h/stat.h to provide it), so define them here the same way
   Salvia's src/compat/dirent.h does. */
#ifndef S_IFMT
#define S_IFMT _S_IFMT
#endif
#ifndef S_IFDIR
#define S_IFDIR _S_IFDIR
#endif
#ifndef S_IFREG
#define S_IFREG _S_IFREG
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

/* Silence "unreferenced local function has been removed" for the static
   helpers below when a translation unit does not use all of them. */
#if defined(_MSC_VER)
#pragma warning(disable:4505)
#endif

struct dirent {
	char d_name[MAX_PATH];
};

typedef struct DIR {
	HANDLE handle;
	WIN32_FIND_DATAA data;
	int first;
	struct dirent ent;
} DIR;

static DIR *opendir(const char *name)
{
	DIR *d;
	char pattern[MAX_PATH];
	size_t len;

	if (name == NULL || name[0] == '\0')
		return NULL;

	len = strlen(name);
	if (len == 0 || len + 3 >= MAX_PATH)
		return NULL;

	memcpy(pattern, name, len + 1);
	if (pattern[len - 1] != '\\' && pattern[len - 1] != '/')
		pattern[len++] = '\\';
	pattern[len++] = '*';
	pattern[len] = '\0';

	d = (DIR *)malloc(sizeof(DIR));
	if (d == NULL)
		return NULL;

	d->handle = FindFirstFileA(pattern, &d->data);
	if (d->handle == INVALID_HANDLE_VALUE) {
		free(d);
		return NULL;
	}
	d->first = 1;
	return d;
}

static struct dirent *readdir(DIR *d)
{
	if (d == NULL || d->handle == INVALID_HANDLE_VALUE)
		return NULL;

	if (d->first)
		d->first = 0;
	else if (!FindNextFileA(d->handle, &d->data))
		return NULL;

	strncpy(d->ent.d_name, d->data.cFileName, MAX_PATH - 1);
	d->ent.d_name[MAX_PATH - 1] = '\0';
	return &d->ent;
}

static int closedir(DIR *d)
{
	if (d == NULL)
		return -1;
	if (d->handle != INVALID_HANDLE_VALUE)
		FindClose(d->handle);
	free(d);
	return 0;
}

#endif /* ATARI800_LIBRETRO_DIRENT_H */
