/***************************************************************************
 *   Copyright (C) 2007 PCSX-df Team                                       *
 *   Copyright (C) 2009 Wei Mingzhi                                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

#include "psxcommon.h"
#include "plugins.h"
#include "cdrom.h"
#include "cdriso.h"

#ifdef _XBOX
#include <xtl.h>
#include <process.h>
#include <sys/time.h>
#else
#ifdef _WIN32
#include <process.h>
#include <sys/time.h>
#include <unistd.h>
#include <windows.h>

#endif
#endif

#ifdef USE_CHD
#include <libchdr/chd.h>
/* CHD stores 2352 bytes of sector data + 96 bytes of subcode per "frame" */
#define CHD_CD_SECTOR_DATA      2352
#define CHD_CD_SUBCODE_DATA     96
#define CHD_CD_FRAME_SIZE       (CHD_CD_SECTOR_DATA + CHD_CD_SUBCODE_DATA)
#define CHD_CD_TRACK_PADDING    4
#endif

static FILE *cdHandle = NULL;
static FILE *subHandle = NULL;

static boolean subChanMixed = FALSE;
static boolean subChanRaw = FALSE;

static unsigned char cdbuffer[CD_FRAMESIZE_RAW];
static unsigned char subbuffer[SUB_FRAMESIZE];

#define MODE1_DATA_SIZE			2048

static boolean isMode1ISO = FALSE;

static boolean playing = FALSE;
static boolean cddaBigEndian = FALSE;
static unsigned int cddaCurPos = 0;

char* CALLBACK CDR__getDriveLetter(void);
long CALLBACK CDR__configure(void);
long CALLBACK CDR__test(void);
void CALLBACK CDR__about(void);
long CALLBACK CDR__setfilename(char *filename);
long CALLBACK CDR__getStatus(struct CdrStat *stat);

extern void *hCDRDriver;

struct trackinfo {
	enum {DATA=1, CDDA} type;
	u8 start[3];		// MSF-format
	u8 length[3];		// MSF-format
};

#define MAXTRACKS 100 /* How many tracks can a CD hold? */

static int numtracks = 0;
static struct trackinfo ti[MAXTRACKS];

/* Multi-FILE cue support.
 *
 * A .cue may reference more than one .bin (redump multi-track rips produce
 * one .bin per track). In that case we need a FILE* per unique .bin and,
 * for each track, (a) which handle to read from and (b) where in the disc
 * (LBA, without lead-in) that file's data starts.
 *
 *   ti_handle[K]           - handle for track K's data. NULL means "use
 *                            cdHandle" (the default / single-FILE case).
 *   ti_fileLBAoffset[K]    - disc-LBA-without-lead-in at which ti_handle[K]'s
 *                            data begins. 0 for single-FILE cues.
 *
 * All additional handles (everything beyond cdHandle) are tracked in
 * cdExtraHandles[] so ISOclose/ISOshutdown can close them. cdHandle itself
 * remains closed by the existing paths.
 */
static FILE         *ti_handle[MAXTRACKS];
static unsigned int  ti_fileLBAoffset[MAXTRACKS];
static FILE         *cdExtraHandles[MAXTRACKS];
static int           cdExtraHandleCount = 0;

static void CloseExtraCdHandles(void) {
	int i;
	for (i = 0; i < cdExtraHandleCount; i++) {
		if (cdExtraHandles[i] != NULL) {
			fclose(cdExtraHandles[i]);
			cdExtraHandles[i] = NULL;
		}
	}
	cdExtraHandleCount = 0;
	memset(ti_handle, 0, sizeof(ti_handle));
	memset(ti_fileLBAoffset, 0, sizeof(ti_fileLBAoffset));
}

#ifdef USE_CHD
static chd_file      *chdFile        = NULL;
static unsigned char *chdHunkBuf     = NULL;
static unsigned int   chdHunkBytes   = 0;
static int            chdCurrentHunk = -1;
/* Per-track LBA (disc-relative, 0-based) where track content begins and the
 * corresponding CHD sector index at which that content lives inside the CHD. */
static int            chdTrackLBA[MAXTRACKS];
static int            chdTrackCHDSector[MAXTRACKS];
#endif

// get a sector from a msf-array
__inline unsigned int msf2sec(char *msf) {
	return ((msf[0] * 60 + msf[1]) * 75) + msf[2];
}

__inline void sec2msf(unsigned int s, char *msf) {
	msf[0] = s / 75 / 60;
	s = s - msf[0] * 75 * 60;
	msf[1] = s / 75;
	s = s - msf[1] * 75;
	msf[2] = s;
}

// divide a string of xx:yy:zz into m, s, f
__inline static void tok2msf(char *time, char *msf) {
	char *token;

	token = strtok(time, ":");
	if (token) {
		msf[0] = atoi(token);
	}
	else {
		msf[0] = 0;
	}

	token = strtok(NULL, ":");
	if (token) {
		msf[1] = atoi(token);
	}
	else {
		msf[1] = 0;
	}

	token = strtok(NULL, ":");
	if (token) {
		msf[2] = atoi(token);
	}
	else {
		msf[2] = 0;
	}
}

// this function tries to get the .toc file of the given .bin
// the necessary data is put into the ti (trackinformation)-array
static int parsetoc(const char *isofile) {
	char			tocname[MAXPATHLEN];
	FILE			*fi;
	char			linebuf[256], dummy[256], name[256];
	char			*token;
	char			time[20], time2[20];
	unsigned int	t;

	numtracks = 0;

	// copy name of the iso and change extension from .bin to .toc
	strncpy(tocname, isofile, sizeof(tocname));
	tocname[MAXPATHLEN - 1] = '\0';
	if (strlen(tocname) >= 4) {
		strcpy(tocname + strlen(tocname) - 4, ".toc");
	}
	else {
		return -1;
	}

	if ((fi = fopen(tocname, "r")) == NULL) {
		// try changing extension to .cue (to satisfy some stupid tutorials)
		strcpy(tocname + strlen(tocname) - 4, ".cue");
		if ((fi = fopen(tocname, "r")) == NULL) {
			// if filename is image.toc.bin, try removing .bin (for Brasero)
			strcpy(tocname, isofile);
			t = strlen(tocname);
			if (t >= 8 && strcmp(tocname + t - 8, ".toc.bin") == 0) {
				tocname[t - 4] = '\0';
				if ((fi = fopen(tocname, "r")) == NULL) {
					return -1;
				}
			}
			else {
				return -1;
			}
		}
	}

	memset(&ti, 0, sizeof(ti));
	cddaBigEndian = TRUE; // cdrdao uses big-endian for CD Audio

	// parse the .toc file
	while (fgets(linebuf, sizeof(linebuf), fi) != NULL) {
		// search for tracks
		strncpy(dummy, linebuf, sizeof(linebuf));
		token = strtok(dummy, " ");

		if (token == NULL) continue;

		if (!strcmp(token, "TRACK")) {
			// get type of track
			token = strtok(NULL, " ");
			numtracks++;

			if (!strncmp(token, "MODE2_RAW", 9)) {
				ti[numtracks].type = DATA;
				sec2msf(2 * 75, ti[numtracks].start); // assume data track on 0:2:0

				// check if this image contains mixed subchannel data
				token = strtok(NULL, " ");
				if (token != NULL && !strncmp(token, "RW_RAW", 6)) {
					subChanMixed = TRUE;
					subChanRaw = TRUE;
				}
			}
			else if (!strncmp(token, "AUDIO", 5)) {
				ti[numtracks].type = CDDA;
			}
		}
		else if (!strcmp(token, "DATAFILE")) {
			if (ti[numtracks].type == CDDA) {
				sscanf(linebuf, "DATAFILE \"%[^\"]\" #%d %8s", name, &t, time2);
				t /= CD_FRAMESIZE_RAW + (subChanMixed ? SUB_FRAMESIZE : 0);
				t += 2 * 75;
				sec2msf(t, (char *)&ti[numtracks].start);
				tok2msf((char *)&time2, (char *)&ti[numtracks].length);
			}
			else {
				sscanf(linebuf, "DATAFILE \"%[^\"]\" %8s", name, time);
				tok2msf((char *)&time, (char *)&ti[numtracks].length);
			}
		}
		else if (!strcmp(token, "FILE")) {
			sscanf(linebuf, "FILE \"%[^\"]\" #%d %8s %8s", name, &t, time, time2);
			tok2msf((char *)&time, (char *)&ti[numtracks].start);
			t /= CD_FRAMESIZE_RAW + (subChanMixed ? SUB_FRAMESIZE : 0);
			t += msf2sec(ti[numtracks].start) + 2 * 75;
			sec2msf(t, (char *)&ti[numtracks].start);
			tok2msf((char *)&time2, (char *)&ti[numtracks].length);
		}
	}

	fclose(fi);

	return 0;
}

// this function tries to get the .cue file of the given .bin
// the necessary data is put into the ti (trackinformation)-array
//
// Supports single-FILE cues (classic: one .bin with all tracks, INDEX values
// are disc-relative) and multi-FILE cues (redump style: one .bin per track,
// each FILE directive starts a new sub-address space and INDEX values are
// file-relative). The input path may be either a .cue or a .bin sibling of
// a .cue; when a .cue is passed directly, cdHandle (opened by the caller on
// the .cue text) is reopened to the first .bin referenced by FILE.
static int parsecue(const char *isofile) {
	char			cuename[MAXPATHLEN];
	char			cuedir[MAXPATHLEN];
	char			binfullpath[MAXPATHLEN];
	FILE			*fi;
	char			*token;
	char			time[20];
	char			*tmp;
	char			linebuf[256], dummy[256];
	unsigned int	t;
	int				input_is_cue = 0;
	size_t			isolen;

	/* Running state of the FILE currently in effect while parsing. Any TRACK
	 * we see inherits these; they advance on every FILE directive. */
	FILE			*currentFileHandle = NULL;   /* NULL => falls back to cdHandle */
	unsigned int	 currentFileLBA    = 0;       /* disc-LBA (no lead-in) where this file's data starts */
	int				 filesOpened       = 0;

	numtracks = 0;
	CloseExtraCdHandles();

	// copy name of the iso and change extension from .bin to .cue
	strncpy(cuename, isofile, sizeof(cuename));
	cuename[MAXPATHLEN - 1] = '\0';
	isolen = strlen(cuename);
	if (isolen >= 4) {
		/* Detect if the caller already passed a .cue path (case-insensitive).
		 * Modern frontends (libretro) pass the .cue directly, not the .bin. */
		if (cuename[isolen - 4] == '.' &&
		    (cuename[isolen - 3] == 'c' || cuename[isolen - 3] == 'C') &&
		    (cuename[isolen - 2] == 'u' || cuename[isolen - 2] == 'U') &&
		    (cuename[isolen - 1] == 'e' || cuename[isolen - 1] == 'E')) {
			input_is_cue = 1;
			/* cuename already points to the .cue; leave as-is */
		} else {
			strcpy(cuename + isolen - 4, ".cue");
		}
	}
	else {
		return -1;
	}

	if ((fi = fopen(cuename, "r")) == NULL) {
		return -1;
	}

	/* Cue directory (used to resolve relative .bin paths from FILE). */
	{
		const char *sep1 = strrchr(cuename, '/');
		const char *sep2 = strrchr(cuename, '\\');
		const char *sep  = (sep1 > sep2) ? sep1 : sep2;
		if (sep != NULL) {
			size_t dirlen = (size_t)(sep - cuename) + 1;
			if (dirlen >= sizeof(cuedir)) dirlen = sizeof(cuedir) - 1;
			memcpy(cuedir, cuename, dirlen);
			cuedir[dirlen] = '\0';
		} else {
			cuedir[0] = '\0';
		}
	}

	// Some stupid tutorials wrongly tell users to use cdrdao to rip a
	// "bin/cue" image, which is in fact a "bin/toc" image. So let's check
	// that...
	if (fgets(linebuf, sizeof(linebuf), fi) != NULL) {
		if (!strncmp(linebuf, "CD_ROM_XA", 9)) {
			// Don't proceed further, as this is actually a .toc file rather
			// than a .cue file.
			fclose(fi);
			return parsetoc(isofile);
		}
		fseek(fi, 0, SEEK_SET);
	}

	memset(&ti, 0, sizeof(ti));

	while (fgets(linebuf, sizeof(linebuf), fi) != NULL) {
		strncpy(dummy, linebuf, sizeof(linebuf));
		token = strtok(dummy, " \t");

		if (token == NULL) {
			continue;
		}

		if (!strcmp(token, "FILE")) {
			/* Parse the quoted filename from the raw line (strtok clobbers
			 * spaces in the copy). */
			char	binrel[MAXPATHLEN];
			char	*q1, *q2;
			int		isAbs;

			binrel[0] = '\0';
			q1 = strchr(linebuf, '"');
			if (q1 != NULL) {
				q2 = strchr(q1 + 1, '"');
				if (q2 != NULL && q2 > q1 + 1) {
					size_t n = (size_t)(q2 - q1 - 1);
					if (n >= sizeof(binrel)) n = sizeof(binrel) - 1;
					memcpy(binrel, q1 + 1, n);
					binrel[n] = '\0';
				}
			}
			if (binrel[0] == '\0') {
				continue;  /* malformed FILE line; skip */
			}

			/* Resolve to full path: absolute stays, relative joins cuedir. */
			isAbs = (binrel[0] == '/' || binrel[0] == '\\' ||
			         (binrel[0] != '\0' && binrel[1] == ':'));
			if (isAbs) {
				strncpy(binfullpath, binrel, sizeof(binfullpath) - 1);
				binfullpath[sizeof(binfullpath) - 1] = '\0';
			} else {
				strncpy(binfullpath, cuedir, sizeof(binfullpath) - 1);
				binfullpath[sizeof(binfullpath) - 1] = '\0';
				strncat(binfullpath, binrel,
				        sizeof(binfullpath) - strlen(binfullpath) - 1);
			}

			if (filesOpened == 0) {
				/* First FILE directive. Two possible starting states:
				 *   - input_is_cue: cdHandle currently points at the .cue
				 *     text (opened by LoadCdrom); retarget to the real .bin.
				 *   - else (.bin was passed directly): cdHandle is already
				 *     the .bin; keep it, even if FILE names a different one
				 *     (we trust what the frontend gave us).
				 */
				if (input_is_cue) {
					FILE *newCd = fopen(binfullpath, "rb");
					if (newCd != NULL) {
						if (cdHandle != NULL) fclose(cdHandle);
						cdHandle = newCd;
						SysPrintf("[cue->bin: %s] ", binfullpath);
					} else {
						SysPrintf("\nWARN: .cue references missing binary: %s\n",
						          binfullpath);
						/* Fall through; parse tracks anyway. */
					}
				}
				currentFileHandle = NULL;   /* NULL => use cdHandle */
				currentFileLBA    = 0;
			} else {
				/* Subsequent FILE: advance disc-LBA by the previous file's
				 * size, then open the new .bin and track it for close. */
				FILE *prevHandle = (currentFileHandle != NULL) ? currentFileHandle : cdHandle;
				FILE *newHandle;
				unsigned int prev_sectors = 0;

				if (prevHandle != NULL) {
					fseek(prevHandle, 0, SEEK_END);
					prev_sectors = (unsigned int)(ftell(prevHandle) / 2352);
				}
				currentFileLBA += prev_sectors;

				newHandle = fopen(binfullpath, "rb");
				if (newHandle == NULL) {
					SysPrintf("\nWARN: .cue references missing binary: %s\n",
					          binfullpath);
					/* Best-effort: keep currentFileHandle pointing at the
					 * previous file so later reads don't outright crash; the
					 * affected tracks will read garbage but the emulator
					 * stays alive for single-track recovery. */
				} else {
					if (cdExtraHandleCount < MAXTRACKS) {
						cdExtraHandles[cdExtraHandleCount++] = newHandle;
					}
					currentFileHandle = newHandle;
					SysPrintf("[+bin: %s] ", binfullpath);
				}
			}

			filesOpened++;
		}
		else if (!strcmp(token, "TRACK")){
			numtracks++;

			/* Remember which file this track lives in (for reads) and the
			 * disc-LBA at which that file begins (INDEX values below are
			 * file-relative in multi-FILE mode). */
			ti_handle[numtracks]        = currentFileHandle;
			ti_fileLBAoffset[numtracks] = currentFileLBA;

			if (strstr(linebuf, "AUDIO") != NULL) {
				ti[numtracks].type = CDDA;
			}
			else if (strstr(linebuf, "MODE1/2352") != NULL || strstr(linebuf, "MODE2/2352") != NULL) {
				ti[numtracks].type = DATA;
			}
		}
		else if (!strcmp(token, "INDEX")) {
			tmp = strstr(linebuf, "INDEX");
			if (tmp != NULL) {
				tmp += strlen("INDEX") + 3; // 3 - space + numeric index
				while (*tmp == ' ') tmp++;
				if (*tmp != '\n') sscanf(tmp, "%8s", time);
			}

			tok2msf((char *)&time, (char *)&ti[numtracks].start);

			/* Disc-LBA (with lead-in) =
			 *     file-relative-INDEX + file's disc-LBA offset + 2s lead-in.
			 * For single-FILE cues, ti_fileLBAoffset[numtracks] is 0, which
			 * reproduces the legacy behavior exactly. */
			t = msf2sec(ti[numtracks].start) + ti_fileLBAoffset[numtracks] + 2 * 75;
			sec2msf(t, ti[numtracks].start);

			// If we've already seen another track, this is its end
			if (numtracks > 1) {
				t = msf2sec(ti[numtracks].start) - msf2sec(ti[numtracks - 1].start);
				sec2msf(t, ti[numtracks - 1].length);
			}
		}
	}

	fclose(fi);

	/* Fill out the last track's end based on the size of the file that
	 * actually contains it (which may not be cdHandle in multi-FILE mode). */
	if (numtracks >= 1) {
		FILE *lastHandle = (ti_handle[numtracks] != NULL) ? ti_handle[numtracks] : cdHandle;
		if (lastHandle != NULL) {
			unsigned int fsz_sectors;
			fseek(lastHandle, 0, SEEK_END);
			fsz_sectors = (unsigned int)(ftell(lastHandle) / 2352);
			/* end-LBA (with lead-in) = file_offset + file_size + 2s
			 * length = end-LBA - track.start */
			t = fsz_sectors + ti_fileLBAoffset[numtracks] + 2 * 75
			    - msf2sec(ti[numtracks].start);
			sec2msf(t, ti[numtracks].length);
		}
	}

	return 0;
}

// this function tries to get the .ccd file of the given .img
// the necessary data is put into the ti (trackinformation)-array
static int parseccd(const char *isofile) {
	char			ccdname[MAXPATHLEN];
	FILE			*fi;
	char			linebuf[256];
	unsigned int	t;

	numtracks = 0;

	// copy name of the iso and change extension from .img to .ccd
	strncpy(ccdname, isofile, sizeof(ccdname));
	ccdname[MAXPATHLEN - 1] = '\0';
	if (strlen(ccdname) >= 4) {
		strcpy(ccdname + strlen(ccdname) - 4, ".ccd");
	}
	else {
		return -1;
	}

	if ((fi = fopen(ccdname, "r")) == NULL) {
		return -1;
	}

	memset(&ti, 0, sizeof(ti));

	while (fgets(linebuf, sizeof(linebuf), fi) != NULL) {
		if (!strncmp(linebuf, "[TRACK", 6)){
			numtracks++;
		}
		else if (!strncmp(linebuf, "MODE=", 5)) {
			sscanf(linebuf, "MODE=%d", &t);
			ti[numtracks].type = ((t == 0) ? CDDA : DATA);
		}
		else if (!strncmp(linebuf, "INDEX 1=", 8)) {
			sscanf(linebuf, "INDEX 1=%d", &t);
			sec2msf(t + 2 * 75, ti[numtracks].start);

			// If we've already seen another track, this is its end
			if (numtracks > 1) {
				t = msf2sec(ti[numtracks].start) - msf2sec(ti[numtracks - 1].start);
				sec2msf(t, ti[numtracks - 1].length);
			}
		}
	}

	fclose(fi);

	// Fill out the last track's end based on size
	if (numtracks >= 1) {
		fseek(cdHandle, 0, SEEK_END);
		t = ftell(cdHandle) / 2352 - msf2sec(ti[numtracks].start) + 2 * 75;
		sec2msf(t, ti[numtracks].length);
	}

	return 0;
}

// this function tries to get the .mds file of the given .mdf
// the necessary data is put into the ti (trackinformation)-array
static int parsemds(const char *isofile) {
	char			mdsname[MAXPATHLEN];
	FILE			*fi;
	unsigned int	offset, extra_offset, l, i;
	unsigned short	s;

	numtracks = 0;

	// copy name of the iso and change extension from .mdf to .mds
	strncpy(mdsname, isofile, sizeof(mdsname));
	mdsname[MAXPATHLEN - 1] = '\0';
	if (strlen(mdsname) >= 4) {
		strcpy(mdsname + strlen(mdsname) - 4, ".mds");
	}
	else {
		return -1;
	}

	if ((fi = fopen(mdsname, "rb")) == NULL) {
		return -1;
	}

	memset(&ti, 0, sizeof(ti));

	// check if it's a valid mds file
	fread(&i, 1, sizeof(unsigned int), fi);
	i = SWAP32(i);
	if (i != 0x4944454D) {
		// not an valid mds file
		fclose(fi);
		return -1;
	}

	// get offset to session block
	fseek(fi, 0x50, SEEK_SET);
	fread(&offset, 1, sizeof(unsigned int), fi);
	offset = SWAP32(offset);

	// get total number of tracks
	offset += 14;
	fseek(fi, offset, SEEK_SET);
	fread(&s, 1, sizeof(unsigned short), fi);
	s = SWAP16(s);
	numtracks = s;

	// get offset to track blocks
	fseek(fi, 4, SEEK_CUR);
	fread(&offset, 1, sizeof(unsigned int), fi);
	offset = SWAP32(offset);

	// skip lead-in data
	while (1) {
		fseek(fi, offset + 4, SEEK_SET);
		if (fgetc(fi) < 0xA0) {
			break;
		}
		offset += 0x50;
	}

	// check if the image contains mixed subchannel data
	fseek(fi, offset + 1, SEEK_SET);
	subChanMixed = (fgetc(fi) ? TRUE : FALSE);

	// read track data
	for (i = 1; i <= numtracks; i++) {
		fseek(fi, offset, SEEK_SET);

		// get the track type
		ti[i].type = ((fgetc(fi) == 0xA9) ? CDDA : DATA);
		fseek(fi, 8, SEEK_CUR);

		// get the track starting point
		ti[i].start[0] = fgetc(fi);
		ti[i].start[1] = fgetc(fi);
		ti[i].start[2] = fgetc(fi);

		if (i > 1) {
			l = msf2sec(ti[i].start);
			sec2msf(l - 2 * 75, ti[i].start); // ???
		}

		// get the track length
		fread(&extra_offset, 1, sizeof(unsigned int), fi);
		extra_offset = SWAP32(extra_offset);

		fseek(fi, extra_offset + 4, SEEK_SET);
		fread(&l, 1, sizeof(unsigned int), fi);
		l = SWAP32(l);
		sec2msf(l, ti[i].length);

		offset += 0x50;
	}

	fclose(fi);
	return 0;
}

#ifdef USE_CHD
// Parse a .chd file and fill in track information. Returns 0 on success.
static int parsechd(const char *isofile) {
	const chd_header *head;
	int cumulative_chd_sectors = 0;
	int i;

	numtracks = 0;
	chdFile = NULL;
	chdHunkBuf = NULL;
	chdCurrentHunk = -1;

	if (chd_open(isofile, CHD_OPEN_READ, NULL, &chdFile) != CHDERR_NONE) {
		chdFile = NULL;
		return -1;
	}

	head = chd_get_header(chdFile);
	if (head == NULL || head->hunkbytes == 0 ||
	    (head->hunkbytes % CHD_CD_FRAME_SIZE) != 0) {
		chd_close(chdFile);
		chdFile = NULL;
		return -1;
	}

	chdHunkBytes = head->hunkbytes;
	chdHunkBuf = (unsigned char *)malloc(chdHunkBytes);
	if (chdHunkBuf == NULL) {
		chd_close(chdFile);
		chdFile = NULL;
		return -1;
	}

	memset(&ti, 0, sizeof(ti));
	/* CHD stores CDDA samples big-endian; PSX core expects them byte-swapped */
	cddaBigEndian = TRUE;

	for (i = 0; i < MAXTRACKS - 1; i++) {
		char metadata[256];
		char type[16], subtype[16], pgtype[16], pgsub[16];
		int  tracknum = 0, frames = 0, pregap = 0, postgap = 0;
		int  pregap_content;
		int  track_start_lba;

		type[0] = subtype[0] = pgtype[0] = pgsub[0] = 0;

		if (chd_get_metadata(chdFile, CDROM_TRACK_METADATA2_TAG, i,
		                     metadata, sizeof(metadata), NULL, NULL, NULL) == CHDERR_NONE) {
			if (sscanf(metadata, CDROM_TRACK_METADATA2_FORMAT,
			           &tracknum, type, subtype, &frames,
			           &pregap, pgtype, pgsub, &postgap) != 8)
				break;
		}
		else if (chd_get_metadata(chdFile, CDROM_TRACK_METADATA_TAG, i,
		                          metadata, sizeof(metadata), NULL, NULL, NULL) == CHDERR_NONE) {
			if (sscanf(metadata, CDROM_TRACK_METADATA_FORMAT,
			           &tracknum, type, subtype, &frames) != 4)
				break;
		}
		else {
			break; // no more tracks
		}

		if (tracknum != i + 1 || frames < 0 || pregap < 0 || postgap < 0)
			break;

		// pregap is "logically present" in LBA numbering only when PGTYPE is "V"
		// (pregap content is actually stored in the CHD file). Otherwise the
		// pregap is silent pause that is NOT stored in the CHD.
		pregap_content = (pgtype[0] == 'V') ? pregap : 0;

		if (i == 0) {
			// First track always starts at LBA 0 (MSF 00:02:00 with PSX lead-in)
			track_start_lba = 0;
			// Track 1 on PSX is data (first track type comes from metadata)
			if (strncmp(type, "AUDIO", 5) == 0)
				ti[tracknum].type = CDDA;
			else
				ti[tracknum].type = DATA;
		}
		else {
			// Next track starts right after previous track's total length
			int prev_start_lba = msf2sec(ti[tracknum - 1].start) - 2 * 75;
			int prev_length    = msf2sec(ti[tracknum - 1].length);
			track_start_lba = prev_start_lba + prev_length;
			if (strncmp(type, "AUDIO", 5) == 0)
				ti[tracknum].type = CDDA;
			else
				ti[tracknum].type = DATA;
		}

		// Store MSF start (PCSX-R stores it with the standard +2s lead-in offset)
		sec2msf(track_start_lba + 2 * 75, ti[tracknum].start);
		sec2msf(frames, ti[tracknum].length);

		// CHD sector index where this track's content (LBA = track_start_lba) lives.
		// Silent pregap (pgtype != 'V') is not present in the CHD so it doesn't
		// advance the CHD sector cursor; content-filled pregap does.
		chdTrackLBA[tracknum]       = track_start_lba + pregap_content;
		chdTrackCHDSector[tracknum] = cumulative_chd_sectors + pregap_content;

		// Advance the CHD sector cursor by this track's frames, rounded up to
		// the 4-sector track padding that CHD applies at end of each track.
		cumulative_chd_sectors += ((frames + CHD_CD_TRACK_PADDING - 1)
		                           / CHD_CD_TRACK_PADDING) * CHD_CD_TRACK_PADDING;

		numtracks = tracknum;
	}

	if (numtracks == 0) {
		free(chdHunkBuf);
		chdHunkBuf = NULL;
		chd_close(chdFile);
		chdFile = NULL;
		return -1;
	}

	return 0;
}

// Read one 2352-byte sector at the given LBA from the currently open CHD
// into the provided buffer. Returns 0 on success, non-zero on error.
static int readchdsector(int lba, unsigned char *buf) {
	int t, chd_sector, hunknum, offset_in_hunk;

	if (chdFile == NULL || chdHunkBuf == NULL)
		return -1;

	// Find track that owns this LBA (search backwards so higher tracks win)
	t = numtracks;
	while (t > 1 && lba < chdTrackLBA[t])
		t--;
	if (t < 1) t = 1;

	chd_sector = chdTrackCHDSector[t] + (lba - chdTrackLBA[t]);
	if (chd_sector < 0)
		return -1;

	hunknum = (chd_sector * CHD_CD_FRAME_SIZE) / chdHunkBytes;
	offset_in_hunk = (chd_sector * CHD_CD_FRAME_SIZE) % chdHunkBytes;

	if (hunknum != chdCurrentHunk) {
		if (chd_read(chdFile, hunknum, chdHunkBuf) != CHDERR_NONE)
			return -1;
		chdCurrentHunk = hunknum;
	}

	memcpy(buf, chdHunkBuf + offset_in_hunk, CHD_CD_SECTOR_DATA);
	return 0;
}
#endif

// this function tries to get the .sub file of the given .img
static int opensubfile(const char *isoname) {
	char		subname[MAXPATHLEN];

	// copy name of the iso and change extension from .img to .sub
	strncpy(subname, isoname, sizeof(subname));
	subname[MAXPATHLEN - 1] = '\0';
	if (strlen(subname) >= 4) {
		strcpy(subname + strlen(subname) - 4, ".sub");
	}
	else {
		return -1;
	}

	subHandle = fopen(subname, "rb");
	if (subHandle == NULL) {
		return -1;
	}

	return 0;
}

long CALLBACK ISOinit(void) {

	return 0; // do nothing
}

#ifdef USE_CHD
static int IsChdFile(const char *filename) {
	size_t n;
	if (filename == NULL) return 0;
	n = strlen(filename);
	if (n < 4) return 0;
	return (filename[n-4] == '.' &&
	        (filename[n-3] == 'c' || filename[n-3] == 'C') &&
	        (filename[n-2] == 'h' || filename[n-2] == 'H') &&
	        (filename[n-1] == 'd' || filename[n-1] == 'D'));
}

static void ChdCloseAll(void) {
	if (chdFile != NULL) {
		chd_close(chdFile);
		chdFile = NULL;
	}
	if (chdHunkBuf != NULL) {
		free(chdHunkBuf);
		chdHunkBuf = NULL;
	}
	chdCurrentHunk = -1;
}
#endif

static long CALLBACK ISOshutdown(void) {
	CloseExtraCdHandles();
	if (cdHandle != NULL) {
		fclose(cdHandle);
		cdHandle = NULL;
	}
	if (subHandle != NULL) {
		fclose(subHandle);
		subHandle = NULL;
	}
#ifdef USE_CHD
	ChdCloseAll();
#endif
	playing = FALSE;
	return 0;
}

static void PrintTracks(void) {
	int i;

	for (i = 1; i <= numtracks; i++) {
		SysPrintf(_("Track %.2d (%s) - Start %.2d:%.2d:%.2d, Length %.2d:%.2d:%.2d\n"),
			i, (ti[i].type == DATA ? "DATA" : "AUDIO"),
			ti[i].start[0], ti[i].start[1], ti[i].start[2],
			ti[i].length[0], ti[i].length[1], ti[i].length[2]);
	}
}

// This function is invoked by the front-end when opening an ISO
// file for playback
static long CALLBACK ISOopen(void) {
	u32 modeTest = 0;

	if (cdHandle != NULL) {
		return 0; // it's already open
	}

#ifdef USE_CHD
	if (chdFile != NULL) {
		return 0; // already open as CHD
	}

	if (IsChdFile(GetIsoFile())) {
		cddaBigEndian = FALSE;
		subChanMixed = FALSE;
		subChanRaw = FALSE;
		isMode1ISO = FALSE;

		if (parsechd(GetIsoFile()) != 0) {
			return -1;
		}

		SysPrintf(_("Loaded CD Image: %s"), GetIsoFile());
		SysPrintf("[+chd].\n");
		PrintTracks();
		return 0;
	}
#endif

	cdHandle = fopen(GetIsoFile(), "rb");
	if (cdHandle == NULL) {
		return -1;
	}

	SysPrintf(_("Loaded CD Image: %s"), GetIsoFile());

	cddaBigEndian = FALSE;
	subChanMixed = FALSE;
	subChanRaw = FALSE;
	isMode1ISO = FALSE;

	if (parseccd(GetIsoFile()) == 0) {
		SysPrintf("[+ccd]");
	}
	else if (parsemds(GetIsoFile()) == 0) {
		SysPrintf("[+mds]");
	}
	else if (parsecue(GetIsoFile()) == 0) {
		SysPrintf("[+cue]");
	}
	else if (parsetoc(GetIsoFile()) == 0) {
		SysPrintf("[+toc]");
	} else {
		//guess whether it is mode1/2048
		fseek(cdHandle, 0, SEEK_END);
		if(ftell(cdHandle) % 2048 == 0) {
			fseek(cdHandle, 0, SEEK_SET);
			fread(&modeTest, 4, 1, cdHandle);
			if(SWAP32(modeTest)!=0xffffff00) isMode1ISO = TRUE;
		}
		fseek(cdHandle, 0, SEEK_SET);
	}

	if (!subChanMixed && opensubfile(GetIsoFile()) == 0) {
		SysPrintf("[+sub]");
	}

	SysPrintf(".\n");

	PrintTracks();

	return 0;
}

static long CALLBACK ISOclose(void) {
	CloseExtraCdHandles();
	if (cdHandle != NULL) {
		fclose(cdHandle);
		cdHandle = NULL;
	}
	if (subHandle != NULL) {
		fclose(subHandle);
		subHandle = NULL;
	}
#ifdef USE_CHD
	ChdCloseAll();
#endif
	playing = FALSE;
	return 0;
}

// return Starting and Ending Track
// buffer:
//  byte 0 - start track
//  byte 1 - end track
static long CALLBACK ISOgetTN(unsigned char *buffer) {
	buffer[0] = 1;

	if (numtracks > 0) {
		buffer[1] = numtracks;
	}
	else {
		buffer[1] = 1;
	}

	return 0;
}

// return Track Time
// buffer:
//  byte 0 - frame
//  byte 1 - second
//  byte 2 - minute
static long CALLBACK ISOgetTD(unsigned char track, unsigned char *buffer) {
	if( track == 0 ) {
		unsigned int pos, size;
		unsigned char time[3];

#ifdef USE_CHD
		if (chdFile != NULL) {
			unsigned int total_sectors = 0;
			if (numtracks > 0) {
				// MSF start already has +2s lead-in baked in
				total_sectors = msf2sec(ti[numtracks].start) + msf2sec(ti[numtracks].length);
			}
			sec2msf(total_sectors, time);
			buffer[2] = time[0];
			buffer[1] = time[1];
			buffer[0] = time[2];
			return 0;
		}
#endif

		// Vib Ribbon: return size of CD
		// - ex. 20 min, 22 sec, 66 fra
		if (numtracks > 0) {
			/* Multi-FILE-safe path: ti[numtracks].start already has the 2s
			 * lead-in baked in, and .length was computed from the owning
			 * file's size. Works for both single- and multi-FILE cues. */
			unsigned int total_sectors =
				msf2sec(ti[numtracks].start) + msf2sec(ti[numtracks].length);
			sec2msf(total_sectors, time);
		} else {
			pos = ftell( cdHandle );
			fseek( cdHandle, 0, SEEK_END );
			size = ftell( cdHandle );
			fseek( cdHandle, pos, SEEK_SET );

			// relative -> absolute time (+2 seconds)
			size += 150 * 2352;

			sec2msf( size / 2352, time );
		}
		buffer[2] = time[0];
		buffer[1] = time[1];
		buffer[0] = time[2];
	}
	else if (numtracks > 0 && track <= numtracks) {
		buffer[2] = ti[track].start[0];
		buffer[1] = ti[track].start[1];
		buffer[0] = ti[track].start[2];
	}
	else {
		buffer[2] = 0;
		buffer[1] = 2;
		buffer[0] = 0;
	}

	return 0;
}

// decode 'raw' subchannel data ripped by cdrdao
static void DecodeRawSubData(void) {
	unsigned char subQData[12];
	int i;

	memset(subQData, 0, sizeof(subQData));

	for (i = 0; i < 8 * 12; i++) {
		if (subbuffer[i] & (1 << 6)) { // only subchannel Q is needed
			subQData[i >> 3] |= (1 << (7 - (i & 7)));
		}
	}

	memcpy(&subbuffer[12], subQData, 12);
}

// read track
// time: byte 0 - minute; byte 1 - second; byte 2 - frame
// uses bcd format
static long CALLBACK ISOreadTrack(unsigned char *time) {
	int		disc_lba;
	FILE	*h;
	unsigned int fileLBAoff;
	long	offset_lba;

#ifdef USE_CHD
	if (chdFile != NULL) {
		int lba = MSF2SECT(btoi(time[0]), btoi(time[1]), btoi(time[2]));
		if (readchdsector(lba, cdbuffer) != 0)
			return -1;
		return 0;
	}
#endif

	if (cdHandle == NULL) {
		return -1;
	}

	disc_lba   = MSF2SECT(btoi(time[0]), btoi(time[1]), btoi(time[2]));
	h          = cdHandle;
	fileLBAoff = 0;

	/* Route the read to the file that owns this LBA. For single-FILE cues
	 * ti_handle[] stays NULL and ti_fileLBAoffset[] stays 0, so this reduces
	 * to the legacy behavior exactly. */
	if (numtracks > 0) {
		int k;
		for (k = numtracks; k >= 1; k--) {
			int track_start_lba = (int)msf2sec(ti[k].start) - 2 * 75;
			if (track_start_lba <= disc_lba) {
				if (ti_handle[k] != NULL) h = ti_handle[k];
				fileLBAoff = ti_fileLBAoffset[k];
				break;
			}
		}
	}

	offset_lba = (long)disc_lba - (long)fileLBAoff;
	if (offset_lba < 0) offset_lba = 0;

	if (subChanMixed) {
		fseek(h, offset_lba * (CD_FRAMESIZE_RAW + SUB_FRAMESIZE), SEEK_SET);
		fread(cdbuffer, 1, CD_FRAMESIZE_RAW, h);
		fread(subbuffer, 1, SUB_FRAMESIZE, h);

		if (subChanRaw) DecodeRawSubData();
	}
	else {
		if(isMode1ISO) {
			fseek(h, offset_lba * MODE1_DATA_SIZE, SEEK_SET);
			fread(cdbuffer + 12, 1, MODE1_DATA_SIZE, h);
			memset(cdbuffer, 0, 12); //not really necessary, fake mode 2 header
			cdbuffer[0] = (time[0]);
			cdbuffer[1] = (time[1]);
			cdbuffer[2] = (time[2]);
			cdbuffer[3] = 1; //mode 1
		} else {
			fseek(h, offset_lba * CD_FRAMESIZE_RAW, SEEK_SET);
			fread(cdbuffer, 1, CD_FRAMESIZE_RAW, h);
		}

		if (subHandle != NULL) {
			fseek(subHandle, disc_lba * SUB_FRAMESIZE, SEEK_SET);
			fread(subbuffer, 1, SUB_FRAMESIZE, subHandle);

			if (subChanRaw) DecodeRawSubData();
		}
	}

	return 0;
}

// return readed track
static unsigned char * CALLBACK ISOgetBuffer(void) {
	return cdbuffer+12;
}

// plays cdda audio
// sector: byte 0 - minute; byte 1 - second; byte 2 - frame
// does NOT uses bcd format
static long CALLBACK ISOplay(unsigned char *time) {
  if (numtracks <= 1)
    return 0;

    playing = TRUE;
	return 0;
}

// stops cdda audio
static long CALLBACK ISOstop(void) {
	playing = FALSE;
	return 0;
}

// gets subchannel data
static unsigned char* CALLBACK ISOgetBufferSub(void) {
	if (subHandle != NULL || subChanMixed) {
		return subbuffer;
	}

	return NULL;
}

static long CALLBACK ISOgetStatus(struct CdrStat *stat) {
	u32 sect;
	
	CDR__getStatus(stat);
	
	if (playing) {
		stat->Status |= 0x80;
	}
	
	// relative -> absolute time
	sect = cddaCurPos;
	sec2msf(sect, (u8 *)stat->Time);
	
	// BIOS - boot ID (CD type)
	stat->Type = ti[1].type;
	
	return 0;
}

// read CDDA sector into buffer
long CALLBACK ISOreadCDDA(unsigned char m, unsigned char s, unsigned char f, unsigned char *buffer) {
	unsigned char msf[3] = {m, s, f};
	unsigned char *p;

	cddaCurPos = msf2sec(msf);

	msf[0] = itob(msf[0]);
	msf[1] = itob(msf[1]);
	msf[2] = itob(msf[2]);

	if (ISOreadTrack(msf) != 0) return -1;

	p = ISOgetBuffer();
	if (p == NULL) return -1;

	memcpy(buffer, p - 12, CD_FRAMESIZE_RAW); // copy from the beginning of the sector

	if (cddaBigEndian) {
		int i;
		unsigned char tmp;

		for (i = 0; i < CD_FRAMESIZE_RAW / 2; i++) {
			tmp = buffer[i * 2];
			buffer[i * 2] = buffer[i * 2 + 1];
			buffer[i * 2 + 1] = tmp;
		}
	}

	return 0;
}

void cdrIsoInit(void) {
	CDR_init = ISOinit;
	CDR_shutdown = ISOshutdown;
	CDR_open = ISOopen;
	CDR_close = ISOclose;
	CDR_getTN = ISOgetTN;
	CDR_getTD = ISOgetTD;
	CDR_readTrack = ISOreadTrack;
	CDR_getBuffer = ISOgetBuffer;
	CDR_play = ISOplay;
	CDR_stop = ISOstop;
	CDR_getBufferSub = ISOgetBufferSub;
	CDR_getStatus = ISOgetStatus;
	CDR_readCDDA = ISOreadCDDA;

	CDR_getDriveLetter = CDR__getDriveLetter;
	CDR_configure = CDR__configure;
	CDR_test = CDR__test;
	CDR_about = CDR__about;
	CDR_setfilename = CDR__setfilename;

	numtracks = 0;
}

int cdrIsoActive(void) {
#ifdef USE_CHD
	if (chdFile != NULL) return 1;
#endif
	return (cdHandle != NULL);
}
