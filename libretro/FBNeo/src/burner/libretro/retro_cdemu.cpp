#include "retro_common.h"
#include "retro_cdemu.h"
#include "burnint.h"
#include "file/file_path.h"
#include "chd.h"

#define DPRINTF_BUFFER_SIZE 512
char dprintf_buf[DPRINTF_BUFFER_SIZE];
static INT32 __cdecl libretro_dprintf(TCHAR* szFormat, ...)
{
	va_list vp;
	va_start(vp, szFormat);
	int rc = vsnprintf(dprintf_buf, DPRINTF_BUFFER_SIZE, szFormat, vp);
	va_end(vp);

	if (rc >= 0)
		log_cb(RETRO_LOG_INFO, dprintf_buf);

	return rc;
}
#define dprintf libretro_dprintf

const int MAXIMUM_NUMBER_TRACKS = 100;

const int CD_FRAMES_MINUTE = 60 * 75;
const int CD_FRAMES_SECOND =      75;

const int CD_TYPE_NONE     = 1 << 0;
const int CD_TYPE_BINCUE   = 1 << 1;
const int CD_TYPE_CCD      = 1 << 2;
const int CD_TYPE_CHD      = 1 << 3;

static int cd_pregap;

struct MSF { UINT8 M; UINT8 S; UINT8 F; };

struct cdimgTRACK_DATA { UINT8 Control; UINT8 TrackNumber; UINT8 Address[4]; UINT8 EndAddress[4]; TCHAR TrackImage[MAX_PATH]; int FileIndexOffset; };
struct cdimgCDROM_TOC { UINT8 FirstTrack; UINT8 LastTrack; UINT8 ImageType; bool bMultiFile; TCHAR Image[MAX_PATH]; cdimgTRACK_DATA TrackData[MAXIMUM_NUMBER_TRACKS]; };

static cdimgCDROM_TOC* cdimgTOC;

static FILE*  cdimgFile = NULL;
static chd_file* cdimgChdFile = NULL;
static UINT32 cdimgChdBytesPerSector = 0;  // bytes per CD sector in CHD (2352 or 2448)
static UINT32 cdimgChdSectorsPerHunk = 0;  // number of CD sectors per hunk
static UINT8* cdimgChdHunkBuf = NULL;      // cached hunk buffer
static INT32  cdimgChdCurHunk = -1;        // currently cached hunk, -1 = none
static INT32  cdimgAudioFilePos = 0;       // audio read position (sector index within CHD)
static int    cdimgTrack = 0;
static int    cdimgLBA = 0;

bool bCDEmuOkay = false;
CDEmuStatusValue CDEmuStatus;
TCHAR CDEmuImage[MAX_PATH];

static int    cdimgSamples = 0;

static int    re_sync = 0;

// identical to the format used in clonecd .sub files, can use memcpy
struct QData { UINT8 Control; char track; char index; MSF MSFrel; char unused; MSF MSFabs; unsigned short CRC; };

static QData* QChannel = NULL;

// -----------------------------------------------------------------------------

const int cdimgOUT_SIZE = 2352;
static int  cdimgOutputbufferSize = 0;

static short* cdimgOutputbuffer = NULL;

static int cdimgOutputPosition;

NGCDGAME* game;

void NeoCDInfo_Exit() {}

TCHAR* NeoCDInfo_Text(int nText)
{
#ifndef NO_NEOGEO
	if(!game || !IsNeoGeoCD() || !bDrvOkay) return NULL;

	switch(nText) 
	{
		case DRV_NAME:			return game->pszName;
		case DRV_FULLNAME:		return game->pszTitle;
		case DRV_MANUFACTURER:	return game->pszCompany;
		case DRV_DATE:			return game->pszYear;
	}
#endif
	return NULL;
}

int NeoCDInfo_ID() 
{
#ifndef NO_NEOGEO
	if(!game || !IsNeoGeoCD() || !bDrvOkay) return 0;
	return game->id;
#else
	return 0;
#endif
}

/**
 * see src/intf/cd/win32/cd_img.cpp
 */

TCHAR* GetIsoPath()
{
	if (cdimgTOC) {
		return cdimgTOC->Image;
	}

	return NULL;
}

static UINT8 bcd(const UINT8 v)
{
	return ((v >> 4) * 10) + (v & 0x0F);
}

static UINT8 tobcd(const UINT8 v)
{
	return ((v / 10) << 4) | (v % 10);
}

static const UINT8* cdimgLBAToMSF(int LBA)
{
	static UINT8 address[4];

	address[0] = 0;
	address[1] = tobcd(LBA / CD_FRAMES_MINUTE);
	address[2] = tobcd(LBA % CD_FRAMES_MINUTE / CD_FRAMES_SECOND);
	address[3] = tobcd(LBA % CD_FRAMES_SECOND);

	return address;
}

static int cdimgMSFToLBA(const UINT8* address)
{
	int LBA;

	LBA  = bcd(address[3]);
	LBA += bcd(address[2]) * CD_FRAMES_SECOND;
	LBA += bcd(address[1]) * CD_FRAMES_MINUTE;

	return LBA;
}

static const UINT8* dinkLBAToMSF(const int LBA) // not BCD version
{
	static UINT8 address[4];

	address[0] = 0;
	address[1] = LBA                    / CD_FRAMES_MINUTE;
	address[2] = LBA % CD_FRAMES_MINUTE / CD_FRAMES_SECOND;
	address[3] = LBA % CD_FRAMES_SECOND;

	return address;
}

static int dinkMSFToLBA(const UINT8* address)
{
	int LBA;

	LBA  = address[3];
	LBA += address[2] * CD_FRAMES_SECOND;
	LBA += address[1] * CD_FRAMES_MINUTE;

	return LBA;
}

// -----------------------------------------------------------------------------

static void cdimgExitStream()
{
	free(cdimgOutputbuffer);
	cdimgOutputbuffer = NULL;
}

static int cdimgInitStream()
{
	cdimgExitStream();

	cdimgOutputbuffer = (short*)malloc(cdimgOUT_SIZE * 2 * sizeof(short));
	if (cdimgOutputbuffer == NULL)
		return 1;

	return 0;
}

static int cdimgSkip(FILE* h, int samples)
{
	fseek(h, samples * 4, SEEK_CUR);

	return samples * 4;
}

// -----------------------------------------------------------------------------

static void cdimgPrintImageInfo()
{
	bprintf(0, _T("Image file: %s\n"), cdimgTOC->Image);

	bprintf(0, _T("   CD image TOC - "));
	if (cdimgTOC->ImageType == CD_TYPE_CCD)
		bprintf(0, _T("TruRip (.CCD/.SUB/.IMG) format\n"));
	if (cdimgTOC->ImageType == CD_TYPE_BINCUE)
		bprintf(0, _T("Disk At Once (.BIN/.CUE) format\n"));
	if (cdimgTOC->ImageType == CD_TYPE_CHD)
		bprintf(0, _T("Compressed Hunks of Data (.CHD) format\n"));

	for (INT32 trk = cdimgTOC->FirstTrack - 1; trk <= cdimgTOC->LastTrack; trk++) {
		const UINT8* addressUNBCD = dinkLBAToMSF(cdimgMSFToLBA(cdimgTOC->TrackData[trk].Address));

		if (trk != cdimgTOC->LastTrack) {
			bprintf(0, _T("Track %02d: %02d:%02d:%02d\n"), trk + 1, addressUNBCD[1], addressUNBCD[2], addressUNBCD[3]);
		} else {
			bprintf(0, _T("    total running time %02i:%02i:%02i\n"), addressUNBCD[1], addressUNBCD[2], addressUNBCD[3]);
		}
	}
}

static void cdimgAddLastTrack()
{ // Make a fake last-track w/total image size (for bounds checking)
	FILE* h = fopen(cdimgTOC->Image, _T("rb"));
	if (h)
	{
		fseek(h, 0, SEEK_END);
		const UINT8* address = cdimgLBAToMSF(((ftell(h) + 2351) / 2352) + cd_pregap);
		fclose(h);

		cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[1] = address[1];
		cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[2] = address[2];
		cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[3] = address[3];
	}
}

// parse .sub file and build a TOC based in Q sub channel data
static int cdimgParseSubFile()
{
	TCHAR  filename_sub[MAX_PATH];
	int    length = 0;
	QData* Q = 0;
	int    Qsize = 0;
	FILE*  h;
	int    track = 1;

	cdimgTOC->ImageType  = CD_TYPE_CCD;
	cdimgTOC->FirstTrack = 1;

	_tcscpy(filename_sub, CDEmuImage);
	length = _tcslen(filename_sub);

	if (length <= 4 ||
		(!IsFileExt(filename_sub, _T(".ccd")) &&
		 !IsFileExt(filename_sub, _T(".img")) &&
		 !IsFileExt(filename_sub, _T(".sub"))))
	{
		dprintf(_T("*** Bad image: %s\n"), filename_sub);
		return 1;
	}

	_tcscpy(cdimgTOC->Image, CDEmuImage);
	_tcscpy(cdimgTOC->Image + length - 4, _T(".img"));
	//bprintf(0, _T("Image file: %s\n"),cdimgTOC->Image);

	_tcscpy(filename_sub + length - 4, _T(".sub"));
	//bprintf(0, _T("filename_sub: %s\n"),filename_sub);
	h = fopen(filename_sub, _T("rb"));
	if (h == 0)
	{
		dprintf(_T("*** Bad image: %s\n"), filename_sub);
		return 1;
	}

	fseek(h, 0, SEEK_END);

	INT32 subQidx = 0;
	UINT32 subQsize = ftell(h);
	UINT8 *subQdata = (UINT8*)malloc(subQsize);
	memset(subQdata, 0, subQsize);

	//bprintf(0, _T("raw .sub data size: %d\n"), subQsize);
	fseek(h, 0, SEEK_SET);
	fread(subQdata, subQsize, 1, h);
	fclose(h);

	Qsize = (subQsize + 95) / 96 * sizeof(QData);
	Q = QChannel = (QData*)malloc(Qsize);
	memset(Q, 0, Qsize);

	INT32 track_linear = 1;

	while (1)
	{
		subQidx += 12;
		if (subQidx >= subQsize) break;
		memcpy(Q, &subQdata[subQidx], 12);
		subQidx += 12;
		subQidx += 6*12;

		if (Q->index && (Q->Control & 1) && (cdimgTOC->TrackData[bcd(Q->track) - 1].TrackNumber == 0))
		{
			// new track
			track = bcd(Q->track);

			if (track == track_linear) {
				//dprintf(_T("  - Track %i found starting at %02X:%02X:%02X\n"), track, Q->MSFabs.M, Q->MSFabs.S, Q->MSFabs.F);
				//bprintf(0, _T("              contrl: %X  track %X(%d)   indx %X\n"),Q->Control,Q->track,track,Q->index);

				cdimgTOC->TrackData[track - 1].Control = Q->Control; // >> 4;
				cdimgTOC->TrackData[track - 1].TrackNumber = Q->track;
				cdimgTOC->TrackData[track - 1].Address[1] = Q->MSFabs.M;
				cdimgTOC->TrackData[track - 1].Address[2] = Q->MSFabs.S;
				cdimgTOC->TrackData[track - 1].Address[3] = Q->MSFabs.F;
				track_linear++;
			} else {
				//bprintf(0, _T("skipped weird track: %X (%X)\n"), track, Q->track);
			}
		}

		Q++;
	}

	cdimgTOC->LastTrack = track;

	free(subQdata);

	cd_pregap = QChannel[0].MSFabs.F + QChannel[0].MSFabs.S * CD_FRAMES_SECOND + QChannel[0].MSFabs.M * CD_FRAMES_MINUTE;
	//bprintf(0, _T("pregap lba: %d MSF: %d:%d:%d\n"), cd_pregap, QChannel[0].MSFabs.M, QChannel[0].MSFabs.S, QChannel[0].MSFabs.F);

	cdimgAddLastTrack();

	return 0;
}

static int cdimgParseCueFile()
{
	TCHAR  szLine[1024];
	TCHAR  szCurrentFile[MAX_PATH];
	TCHAR* s;
	TCHAR* t;
	FILE*  h;
	int    track = 1;
	int    length;
	int    nFiles = 0; // contador de lineas FILE distintas, para detectar multi-archivo

	cdimgTOC->ImageType  = CD_TYPE_BINCUE;
	cdimgTOC->FirstTrack = 1;
	cdimgTOC->LastTrack  = 1;
	cdimgTOC->bMultiFile = false;

	cdimgTOC->TrackData[0].Address[1] = 0;
	cdimgTOC->TrackData[0].Address[2] = 2;
	cdimgTOC->TrackData[0].Address[3] = 0;

	cd_pregap = 150;

	length = _tcslen(CDEmuImage);
	_tcscpy(cdimgTOC->Image, CDEmuImage);
	_tcscpy(cdimgTOC->Image + length - 4, _T(".bin"));

	szCurrentFile[0] = _T('\0');

	h = fopen(CDEmuImage, _T("rt"));
	if (h == NULL) {
		return 1;
	}

	// --- PASO 1: leer el .cue y guardar TrackImage y FileIndexOffset por pista ---
	while (1) {
		if (fgets(szLine, sizeof(szLine), h) == NULL) {
			break;
		}

		length = _tcslen(szLine);
		while (length && (szLine[length - 1] == _T('\r') || szLine[length - 1] == _T('\n'))) {
			szLine[length - 1] = 0;
			length--;
		}

		s = szLine;

		// FILE: guardar en buffer temporal
		if ((t = LabelCheck(s, _T("FILE"))) != 0) {
			s = t;
			TCHAR* szQuote;
			QuoteRead(&szQuote, NULL, s);
			size_t len = strlen(g_rom_dir);
			if (len > 0 && (g_rom_dir[len-1] == '\\' || g_rom_dir[len-1] == '/')) {
				// Do not add separator if there is one already
				sprintf(szCurrentFile, "%s%s", g_rom_dir, szQuote);
			} else {
				// Add a separator if necessary
				sprintf(szCurrentFile, "%s%c%s", g_rom_dir, PATH_DEFAULT_SLASH_C(), szQuote);
			}
			nFiles++;
			continue;
		}

		// TRACK: asociar szCurrentFile a este track (tanto data como audio)
		if ((t = LabelCheck(s, _T("TRACK"))) != 0) {
			s = t;

			track = _tcstol(s, &t, 10);

			if (track < 1 || track > MAXIMUM_NUMBER_TRACKS) {
				fclose(h);
				return 1;
			}

			if (track < cdimgTOC->FirstTrack) cdimgTOC->FirstTrack = track;
			if (track > cdimgTOC->LastTrack)  cdimgTOC->LastTrack  = track;
			cdimgTOC->TrackData[track - 1].TrackNumber = tobcd(track);

			// Guardar el fichero de esta pista (sirve para data y para audio)
			if (szCurrentFile[0] != _T('\0')) {
				_tcscpy(cdimgTOC->TrackData[track - 1].TrackImage, szCurrentFile);
			}

			s = t;

			if ((t = LabelCheck(s, _T("MODE1/2352"))) != 0) {
				cdimgTOC->TrackData[track - 1].Control = 0x41;
				if (szCurrentFile[0] != _T('\0')) {
					_tcscpy(cdimgTOC->Image, szCurrentFile);
				}
				continue;
			}
			if ((t = LabelCheck(s, _T("AUDIO"))) != 0) {
				cdimgTOC->TrackData[track - 1].Control = 0x01;
				continue;
			}

			fclose(h);
			return 1;
		}

		if ((t = LabelCheck(s, _T("PREGAP"))) != 0) {
			continue;
		}

		// INDEX 00 se ignora (solo nos interesa INDEX 01)
		if ((t = LabelCheck(s, _T("INDEX 00"))) != 0) {
			continue;
		}

		// INDEX 01: guardar la posicion relativa al fichero en FileIndexOffset
		if ((t = LabelCheck(s, _T("INDEX 01"))) != 0) {
			s = t;

			int M, S, F;

			M = _tcstol(s, &t, 10);
			s = t + 1;
			S = _tcstol(s, &t, 10);
			s = t + 1;
			F = _tcstol(s, &t, 10);

			if (M < 0 || M > 100 || S < 0 || S > 59 || F < 0 || F > 74) {
				bprintf(0, _T("Bad M:S:F!\n"));
				fclose(h);
				return 1;
			}

			// Guardar el offset dentro del fichero (en fotogramas, no-BCD)
			cdimgTOC->TrackData[track - 1].FileIndexOffset =
				M * CD_FRAMES_MINUTE + S * CD_FRAMES_SECOND + F;

			continue;
		}
	}

	fclose(h);

	// --- PASO 2: detectar si es multi-archivo y calcular las direcciones absolutas de disco ---

	cdimgTOC->bMultiFile = (nFiles > 1);

	if (!cdimgTOC->bMultiFile) {
		// Fichero unico: los tiempos del .cue son posiciones acumuladas en el disco.
		// FileIndexOffset + cd_pregap = LBA absoluto en disco.
		for (int trk = cdimgTOC->FirstTrack - 1; trk < cdimgTOC->LastTrack; trk++) {
			const UINT8* newaddress = cdimgLBAToMSF(cdimgTOC->TrackData[trk].FileIndexOffset + cd_pregap);
			cdimgTOC->TrackData[trk].Address[1] = newaddress[1];
			cdimgTOC->TrackData[trk].Address[2] = newaddress[2];
			cdimgTOC->TrackData[trk].Address[3] = newaddress[3];
		}
		cdimgAddLastTrack();
	} else {
		// Multi-archivo: cada pista tiene su propio .bin.
		// Los tiempos INDEX 01 del .cue son relativos al fichero, no al disco.
		// Calculamos LBAs absolutas de disco acumulando los tamanhos de cada fichero.
		bprintf(0, _T("CUE multi-archivo detectado (%d ficheros)\n"), nFiles);

		int current_disc_lba = cd_pregap; // la pista 1 empieza en cd_pregap

		for (int trk = cdimgTOC->FirstTrack - 1; trk < cdimgTOC->LastTrack; trk++) {
			// LBA absoluta de disco del INDEX 01 de esta pista
			int disc_index01 = current_disc_lba + cdimgTOC->TrackData[trk].FileIndexOffset;
			const UINT8* addr = cdimgLBAToMSF(disc_index01);
			cdimgTOC->TrackData[trk].Address[1] = addr[1];
			cdimgTOC->TrackData[trk].Address[2] = addr[2];
			cdimgTOC->TrackData[trk].Address[3] = addr[3];

			// Avanzar la LBA de disco por el tamanho completo del fichero de esta pista
			FILE* f = fopen(cdimgTOC->TrackData[trk].TrackImage, _T("rb"));
			if (f) {
				fseek(f, 0, SEEK_END);
				int file_frames = (int)((ftell(f) + 2351) / 2352);
				fclose(f);
				current_disc_lba += file_frames;
			} else {
				bprintf(0, _T("*** No se puede abrir para calcular tamanho: %s\n"),
					cdimgTOC->TrackData[trk].TrackImage);
			}
		}

		// Entrada centinela: LBA total del disco
		const UINT8* addr = cdimgLBAToMSF(current_disc_lba);
		cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[1] = addr[1];
		cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[2] = addr[2];
		cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[3] = addr[3];
	}

	return 0;
}


// -----------------------------------------------------------------------------
// CHD support (libchdr).  The CHD file IS the complete image; sector data is
// served via a one-slot hunk decompression cache.  CD-DA samples are stored
// big-endian in CHD; we re-pack them as little-endian to match the .bin/.cue
// in-memory layout, so the existing play loop (which uses BURN_ENDIAN_SWAP_INT16
// to normalize on big-endian hosts like Xbox 360) works unchanged on both
// little- and big-endian targets.

// Decompress a single CHD sector into a 2352-byte raw mode-1 sector.
//   sector : 0-based sector index inside the CHD file (no pregap offset)
//   dest   : caller-provided buffer, must hold at least 2352 bytes
//   return : 0 on success, non-zero on error
static INT32 cdimgChdReadSector(INT32 sector, UINT8* dest)
{
	if (!cdimgChdFile || !cdimgChdHunkBuf || cdimgChdSectorsPerHunk == 0)
		return 1;

	INT32 hunk = sector / (INT32)cdimgChdSectorsPerHunk;
	INT32 offset_in_hunk = (sector % (INT32)cdimgChdSectorsPerHunk) * (INT32)cdimgChdBytesPerSector;

	if (cdimgChdCurHunk != hunk) {
		chd_error err = chd_read(cdimgChdFile, (UINT32)hunk, cdimgChdHunkBuf);
		if (err != CHDERR_NONE)
			return 1;
		cdimgChdCurHunk = hunk;
	}

	UINT32 copy_size = (cdimgChdBytesPerSector < 2352) ? cdimgChdBytesPerSector : 2352;
	memcpy(dest, cdimgChdHunkBuf + offset_in_hunk, copy_size);
	return 0;
}

// Unified raw-sector reader: hides whether the underlying image is a .bin/.cue
// or a .chd.  LBA is data-track-relative (LBA 0 == first user sector).
static INT32 cdimgReadRawSector(INT32 lba, UINT8* dest)
{
	if (!cdimgTOC)
		return 1;

	if (cdimgTOC->ImageType == CD_TYPE_CHD)
		return cdimgChdReadSector(lba, dest);

	if (!cdimgFile)
		return 1;
	if (fseek(cdimgFile, (long)lba * 2352, SEEK_SET) != 0)
		return 1;
	size_t n = fread(dest, 1, 2352, cdimgFile);
	return (n == 2352) ? 0 : 1;
}

// Decompress audio sectors and write them into cdimgOutputbuffer using the
// little-endian byte layout that the play loop expects.  Returns how many
// sectors were successfully read (0 on hard failure).
static INT32 cdimgChdFillAudioBuffer(INT32 base_sector, INT32 sectors_to_read)
{
	UINT8 sector_buf[2352];
	INT32 read_count = 0;

	UINT8* out_bytes = (UINT8*)cdimgOutputbuffer;

	for (INT32 i = 0; i < sectors_to_read; i++) {
		if (cdimgChdReadSector(base_sector + i, sector_buf) != 0)
			break;

		// CHD CD-DA = big-endian 16-bit PCM stereo. Repack to little-endian
		// so the buffer matches the .bin convention used by the play loop.
		// This is endian-safe: dst[0] = low byte, dst[1] = high byte, always.
		UINT8* dst = out_bytes + i * 2352;
		for (INT32 j = 0; j < 2352; j += 2) {
			dst[j + 0] = sector_buf[j + 1]; // LE low byte
			dst[j + 1] = sector_buf[j + 0]; // LE high byte
		}
		read_count++;
	}
	return read_count;
}

// Parse a "KEY:%d" pattern in a CHD metadata string.  -1 if missing.
static INT32 chd_meta_get_int(const char* meta, const char* key)
{
	const char* p = strstr(meta, key);
	if (!p) return -1;
	return atoi(p + strlen(key));
}

// True if the track described by this CHD metadata entry is a data track
// (TYPE starts with "MODE"); AUDIO returns false.  Defaults to data on
// parse error.
static bool chd_meta_is_data_track(const char* meta)
{
	const char* p = strstr(meta, "TYPE:");
	if (!p) return true;
	const char* type_start = p + 5;
	if (strncmp(type_start, "AUDIO", 5) == 0) return false;
	if (strncmp(type_start, "MODE",  4) == 0) return true;
	return true;
}

static INT32 cdimgParseChdFile()
{
	// In the libretro port TCHAR == char, so CDEmuImage is already an
	// 8-bit path that we can pass directly to chd_open.
	chd_error err = chd_open(CDEmuImage, CHD_OPEN_READ, NULL, &cdimgChdFile);
	if (err != CHDERR_NONE) {
		dprintf(_T("*** Couldn't open .chd file\n"));
		return 1;
	}

	const chd_header* header = chd_get_header(cdimgChdFile);
	if (!header) {
		dprintf(_T("*** Couldn't read CHD header\n"));
		chd_close(cdimgChdFile);
		cdimgChdFile = NULL;
		return 1;
	}

	cdimgTOC->ImageType = CD_TYPE_CHD;
	cdimgTOC->bMultiFile = false;
	_tcscpy(cdimgTOC->Image, CDEmuImage);

	// Standard CD pregap (2 seconds = 150 sectors).  The CHD stores the
	// data image without the pregap so the disc-side LBA accounting must
	// add it back manually.
	cd_pregap = 150;

	// Detect CD sector layout encoded in the CHD hunk size.
	if (       header->hunkbytes % 2448 == 0) {
		cdimgChdBytesPerSector = 2448;
	} else if (header->hunkbytes % 2352 == 0) {
		cdimgChdBytesPerSector = 2352;
	} else if (header->hunkbytes % 2048 == 0) {
		cdimgChdBytesPerSector = 2048;
	} else {
		cdimgChdBytesPerSector = header->hunkbytes;
	}
	cdimgChdSectorsPerHunk = header->hunkbytes / cdimgChdBytesPerSector;

	cdimgChdHunkBuf = (UINT8*)malloc(header->hunkbytes);
	if (!cdimgChdHunkBuf) {
		dprintf(_T("*** Out of memory for CHD hunk buffer\n"));
		chd_close(cdimgChdFile);
		cdimgChdFile = NULL;
		return 1;
	}
	cdimgChdCurHunk = -1;

	// Walk the CHD track metadata to rebuild the disc TOC.  V2 metadata
	// (CHT2) carries PREGAP/POSTGAP; V1 (CHTR) is the legacy fallback.
	char   meta_buf[256];
	UINT32 meta_resultlen = 0;
	UINT8  meta_flags     = 0;
	INT32  total_sectors  = 0;
	INT32  trk            = 0;
	UINT32 chd_sector_pos = 0;
	UINT32 cd_lba         = 0;

	for (INT32 idx = 0; idx < 99; idx++) {
		err = chd_get_metadata(cdimgChdFile, CDROM_TRACK_METADATA2_TAG, idx,
			meta_buf, sizeof(meta_buf) - 1, &meta_resultlen, NULL, &meta_flags);
		if (err != CHDERR_NONE || meta_resultlen == 0) {
			err = chd_get_metadata(cdimgChdFile, CDROM_TRACK_METADATA_TAG, idx,
				meta_buf, sizeof(meta_buf) - 1, &meta_resultlen, NULL, &meta_flags);
		}
		if (err != CHDERR_NONE || meta_resultlen == 0)
			break;

		meta_buf[meta_resultlen] = '\0';

		INT32 frames  = chd_meta_get_int(meta_buf, "FRAMES:");
		if (frames <= 0) frames = 0;
		INT32 pregap  = chd_meta_get_int(meta_buf, "PREGAP:");
		if (pregap < 0) pregap = 0;
		INT32 postgap = chd_meta_get_int(meta_buf, "POSTGAP:");
		if (postgap < 0) postgap = 0;
		bool is_data  = chd_meta_is_data_track(meta_buf);

		// CHD stores tracks aligned to a 4-frame boundary.
		if (chd_sector_pos % 4)
			chd_sector_pos += 4 - (chd_sector_pos % 4);

		if (pregap > 0)
			cd_lba += pregap;

		cdimgTOC->TrackData[trk].Control     = is_data ? 0x41 : 0x01;
		cdimgTOC->TrackData[trk].TrackNumber = tobcd(trk + 1);

		const UINT8* track_start_msf = cdimgLBAToMSF(cd_lba);
		cdimgTOC->TrackData[trk].Address[0] = 0;
		cdimgTOC->TrackData[trk].Address[1] = track_start_msf[1];
		cdimgTOC->TrackData[trk].Address[2] = track_start_msf[2];
		cdimgTOC->TrackData[trk].Address[3] = track_start_msf[3];

		cd_lba += frames;
		chd_sector_pos += frames;
		if (postgap > 0)
			cd_lba += postgap;

		total_sectors = cd_lba;
		trk++;

		if (trk >= MAXIMUM_NUMBER_TRACKS)
			break;
	}

	if (trk == 0) {
		// No track metadata: assume a single data track covering the whole image.
		trk = 1;
		total_sectors = (INT32)(header->totalhunks) * cdimgChdSectorsPerHunk;

		cdimgTOC->TrackData[0].Control     = 0x41;
		cdimgTOC->TrackData[0].TrackNumber = tobcd(1);
		const UINT8* start_msf = cdimgLBAToMSF(cd_pregap);
		cdimgTOC->TrackData[0].Address[0]  = 0;
		cdimgTOC->TrackData[0].Address[1]  = start_msf[1];
		cdimgTOC->TrackData[0].Address[2]  = start_msf[2];
		cdimgTOC->TrackData[0].Address[3]  = start_msf[3];

		const UINT8* end_msf = cdimgLBAToMSF(total_sectors + cd_pregap);
		cdimgTOC->TrackData[0].EndAddress[0] = 0;
		cdimgTOC->TrackData[0].EndAddress[1] = end_msf[1];
		cdimgTOC->TrackData[0].EndAddress[2] = end_msf[2];
		cdimgTOC->TrackData[0].EndAddress[3] = end_msf[3];
	} else {
		for (INT32 i = 0; i < trk; i++) {
			UINT32 end_lba = (i + 1 < trk)
				? (UINT32)cdimgMSFToLBA(cdimgTOC->TrackData[i + 1].Address)
				: (UINT32)total_sectors;
			const UINT8* end_msf = cdimgLBAToMSF(end_lba);
			cdimgTOC->TrackData[i].EndAddress[0] = 0;
			cdimgTOC->TrackData[i].EndAddress[1] = end_msf[1];
			cdimgTOC->TrackData[i].EndAddress[2] = end_msf[2];
			cdimgTOC->TrackData[i].EndAddress[3] = end_msf[3];
		}
	}

	cdimgTOC->FirstTrack = 1;
	cdimgTOC->LastTrack  = trk;

	// Lead-out track entry — used as the right-edge bound by cdimgFindTrack
	// and end-of-track checks in the audio path.
	UINT32 leadout_lba = (UINT32)total_sectors + (UINT32)cd_pregap;
	const UINT8* leadout_msf = cdimgLBAToMSF(leadout_lba);
	cdimgTOC->TrackData[trk].Control     = 0x41;
	cdimgTOC->TrackData[trk].TrackNumber = 0xAA;
	cdimgTOC->TrackData[trk].Address[0]  = 0;
	cdimgTOC->TrackData[trk].Address[1]  = leadout_msf[1];
	cdimgTOC->TrackData[trk].Address[2]  = leadout_msf[2];
	cdimgTOC->TrackData[trk].Address[3]  = leadout_msf[3];

	bprintf(0, _T("CHD: %u hunks, %u bytes/hunk (%u sectors/hunk, %u bytes/sector), %d tracks\n"),
		(UINT32)header->totalhunks, header->hunkbytes,
		cdimgChdSectorsPerHunk, cdimgChdBytesPerSector, trk);

	return 0;
}

// -----------------------------------------------------------------------------

static int cdimgExit()
{
	cdimgExitStream();

	if (cdimgFile)
		fclose(cdimgFile);
	cdimgFile = NULL;

	if (cdimgChdFile)
		chd_close(cdimgChdFile);
	cdimgChdFile = NULL;

	if (cdimgChdHunkBuf)
		free(cdimgChdHunkBuf);
	cdimgChdHunkBuf = NULL;
	cdimgChdCurHunk = -1;
	cdimgChdBytesPerSector = 0;
	cdimgChdSectorsPerHunk = 0;

	cdimgTrack = 0;
	cdimgLBA = 0;

	if (cdimgTOC)
		free(cdimgTOC);
	cdimgTOC = NULL;

	free(QChannel);
	QChannel = NULL;

	return 0;
}

static int cdimgInit()
{
	re_sync = 0;

	cdimgTOC = (cdimgCDROM_TOC*)malloc(sizeof(cdimgCDROM_TOC));
	if (cdimgTOC == NULL)
		return 1;

	memset(cdimgTOC, 0, sizeof(cdimgCDROM_TOC));

	cdimgTOC->ImageType = CD_TYPE_NONE;

	TCHAR* filename = ExtractFilename(CDEmuImage);

	if (_tcslen(filename) < 4)
		return 1;

	if (IsFileExt(filename, _T(".cue")))
	{
		if (cdimgParseCueFile())
		{
			dprintf(_T("*** Couldn't parse .cue file\n"));
			cdimgExit();

			return 1;
		}

	} else
	if (IsFileExt(filename, _T(".ccd")))
	{
		if (cdimgParseSubFile())
		{
			dprintf(_T("*** Couldn't parse .sub file\n"));
			cdimgExit();

			return 1;
		}

	} else
	if (IsFileExt(filename, _T(".chd")))
	{
		if (cdimgParseChdFile())
		{
			dprintf(_T("*** Couldn't parse .chd file\n"));
			cdimgExit();

			return 1;
		}

	}
	else
	{
		dprintf(_T("*** Couldn't find .img / .bin file\n"));
		cdimgExit();

		return 1;
	}

	cdimgPrintImageInfo();

	CDEmuStatus = idle;

	cdimgInitStream();

	cdimgLBA++;

	// Validate the CD by scanning the ISO-9660 volume descriptor at sector 16.
	if (cdimgTOC->ImageType == CD_TYPE_CHD)
	{
		// CHD: read a full 2352-byte raw sector and look at the user-data area.
		// CD001 lives at byte 1 of the 2048-byte user data, i.e. byte 17 of the
		// 2352-byte sector.
		UINT8 buf[2352];
		if (cdimgReadRawSector(16, buf) == 0)
		{
			if (strncmp("CD001", (const char*)(buf + 16 + 1), 5) != 0)
				dprintf(_T("*** Bad CD!\n"));
		}
	}
	else
	{
		char buf[2048];
		FILE* h = fopen(cdimgTOC->Image, _T("rb"));

		if (h)
		{
			if (fseek(h, 16 * 2352 + 16, SEEK_SET) == 0)
			{
				if (fread(buf, 1, 2048, h) == 2048)
				{
					if (strncmp("CD001", buf + 1, 5) == 0)
					{
						buf[48] = 0;
						/* BurnDrvFindMedium(buf + 40); */
					}
					else
						dprintf(_T("*** Bad CD!\n"));
				}
			}

			fclose(h);
		}

		//CDEmuPrintCDName();
	}

	return 0;
}

static void cdimgCloseFile()
{
	if (cdimgFile)
	{
		fclose(cdimgFile);
		cdimgFile = NULL;
	}
}

static int cdimgStop()
{
	cdimgCloseFile();
	CDEmuStatus = idle;

	return 0;
}

static int cdimgFindTrack(int LBA)
{
	int trk = 0;
	for (trk = cdimgTOC->FirstTrack - 1; trk < cdimgTOC->LastTrack; trk++)
		if (LBA < cdimgMSFToLBA(cdimgTOC->TrackData[trk + 1].Address))
			break;
	return trk;
}

static int cdimgPlayLBA(int LBA) // audio play start
{
	cdimgStop();

	if (QChannel != NULL) { // .CCD dump w/.SUB
		if (QChannel[LBA].Control & 0x40)
			return 1;
	} else { // .BIN/.CUE dump
		if (cdimgTOC->TrackData[cdimgFindTrack(LBA)].Control & 0x40)
			return 1;
	}

	cdimgLBA = LBA;

	cdimgTrack = cdimgFindTrack(cdimgLBA);

	if (cdimgTrack >= cdimgTOC->LastTrack)
		return 1;

	bprintf(PRINT_IMPORTANT, _T("    playing track %2i\n"), cdimgTrack + 1);

	if (cdimgTOC->ImageType == CD_TYPE_CHD) {
		// CHD: sector 0 of the CHD == first sector of the data track, same
		// origin as a .bin file.  Audio playback starts at (disc LBA - pregap).
		INT32 base = cdimgLBA - cd_pregap;
		if (base < 0) base = 0;
		cdimgAudioFilePos = base;

		INT32 sectors_to_read = (cdimgOUT_SIZE * 4) / 2352;
		INT32 read_count = cdimgChdFillAudioBuffer(base, sectors_to_read);
		if (read_count == 0) {
			cdimgStop();
			return 1;
		}
		cdimgAudioFilePos += read_count;
		// cdimgOutputbufferSize is counted in 4-byte (stereo s16) units, same
		// as the value fread returns for the .bin path below.
		cdimgOutputbufferSize = read_count * (2352 / 4);
	} else if (cdimgTOC->bMultiFile) {
		// Multi-archivo: abrir el fichero .bin especifico de esta pista de audio
		cdimgFile = fopen(cdimgTOC->TrackData[cdimgTrack].TrackImage, _T("rb"));
		if (cdimgFile == NULL) {
			bprintf(0, _T("*** No se puede abrir pista de audio: %s\n"),
				cdimgTOC->TrackData[cdimgTrack].TrackImage);
			return 1;
		}
		// El seek dentro del fichero es: LBA_disco - LBA_inicio_fichero_en_disco
		// LBA_inicio_fichero = disc_index01 - FileIndexOffset
		int disc_index01 = cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack].Address);
		int disc_file_start = disc_index01 - cdimgTOC->TrackData[cdimgTrack].FileIndexOffset;
		int file_frame = cdimgLBA - disc_file_start;
		if (file_frame > 0)
			cdimgSkip(cdimgFile, file_frame * (44100 / CD_FRAMES_SECOND));
		if ((cdimgOutputbufferSize = fread(cdimgOutputbuffer, 4, cdimgOUT_SIZE, cdimgFile)) <= 0)
			return 1;
	} else {
		// Fichero unico: comportamiento original
		cdimgFile = fopen(cdimgTOC->Image, _T("rb"));
		if (cdimgFile == NULL)
			return 1;
		// advance if we're not starting at the beginning of a CD
		if (cdimgLBA > cd_pregap)
			cdimgSkip(cdimgFile, (cdimgLBA - cd_pregap) * (44100 / CD_FRAMES_SECOND));
		if ((cdimgOutputbufferSize = fread(cdimgOutputbuffer, 4, cdimgOUT_SIZE, cdimgFile)) <= 0)
			return 1;
	}

	cdimgOutputPosition = 0;

	cdimgSamples = 0;
	// this breaks states, commenting for now just in-case. -dink
	//cdimgLBA = cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack].Address); // start at the beginning of track
	CDEmuStatus = playing;

	return 0;
}

static int cdimgPlay(UINT8 M, UINT8 S, UINT8 F)
{
	const UINT8 address[] = { 0, M, S, F };

	const UINT8* displayaddress = dinkLBAToMSF(cdimgMSFToLBA(address));
	dprintf(_T("    play %02i:%02i:%02i\n"), displayaddress[1], displayaddress[2], displayaddress[3]);

	return cdimgPlayLBA(cdimgMSFToLBA(address));
}

static int cdimgLoadSector(int LBA, char* pBuffer)
{
	if (CDEmuStatus == playing) return 0; // data loading

	INT32 originalLBA = LBA;
	if (CDEmuStatus == seeking) {
		LBA -= cd_pregap; // when seeking, we must account for pregap
		re_sync = 1;
	}

	// CHD path: serve the sector via the unified reader.  No FILE*/fseek
	// state to maintain — the hunk cache inside cdimgChdReadSector takes
	// care of locality.
	if (cdimgTOC && cdimgTOC->ImageType == CD_TYPE_CHD) {
		if (cdimgReadRawSector(LBA, (UINT8*)pBuffer) != 0) {
			dprintf(_T("*** couldn't read sector (LBA %08u)\n"), originalLBA);
			return 0;
		}
		CDEmuStatus = reading;
		cdimgLBA = LBA + 1;
		cdimgTrack = cdimgFindTrack(LBA + cd_pregap);
		return cdimgLBA;
	}

	if (LBA != cdimgLBA || cdimgFile == NULL || re_sync)
	{
		re_sync = 0;

		if (cdimgFile == NULL)
		{
			cdimgStop();

			cdimgFile = fopen(cdimgTOC->Image, _T("rb"));
			if (cdimgFile == NULL)
				return 0;
		}

		//bprintf(PRINT_IMPORTANT, _T("    loading data at LBA %08u 0x%08X\n"), (LBA - cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack].Address)) * 2352, LBA * 2352);

		if (fseek(cdimgFile, (LBA) * 2352, SEEK_SET))
		{
			dprintf(_T("*** couldn't seek (LBA %08u)\n"), LBA);

			//cdimgStop(); // stopping here will break ssrpg,
			// game will seek away & recover from this.

			return 0;
		}

		CDEmuStatus = reading;
	}

	//dprintf(_T("    reading LBA %08i 0x%08X"), LBA, ftell(cdimgFile));

	cdimgLBA = cdimgMSFToLBA(cdimgTOC->TrackData[0].Address) + (ftell(cdimgFile) + 2351) / 2352 - cd_pregap;

	bool status = (fread(pBuffer, 1, 2352, cdimgFile) <= 0);

	if (status)
	{
		dprintf(_T("*** couldn't read from file - iso corrupt or truncated?\n"));

		cdimgStop();

		return 0;
	}
	// dprintf(_T("    [ %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X ]\n"), pBuffer[0], pBuffer[1], pBuffer[2], pBuffer[3], pBuffer[4], pBuffer[5], pBuffer[6], pBuffer[7], pBuffer[8], pBuffer[9], pBuffer[10], pBuffer[11], pBuffer[12], pBuffer[13], pBuffer[14], pBuffer[15]);

	cdimgLBA++;

	return cdimgLBA;
}

static UINT8* cdimgReadTOC(int track)
{
	static UINT8 TOCEntry[4];

	memset(&TOCEntry, 0, sizeof(TOCEntry));

	if (track == CDEmuTOC_FIRSTLAST)
	{
		TOCEntry[0] = tobcd(cdimgTOC->FirstTrack - 1);
		TOCEntry[1] = tobcd(cdimgTOC->LastTrack);
		TOCEntry[2] = 0;
		TOCEntry[3] = 0;

		return TOCEntry;
	}
	if (track == CDEmuTOC_LASTMSF)
	{
		TOCEntry[0] = cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[1];
		TOCEntry[1] = cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[2];
		TOCEntry[2] = cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[3];

		TOCEntry[3] = 0;

		return TOCEntry;
	}
	if (track == CDEmuTOC_FIRSTINDEX)
	{
		if (cdimgLBA < cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTOC->FirstTrack].Address))
		{
			const UINT8* addressUNBCD = dinkLBAToMSF(cdimgLBA);
			UINT8 index = ((addressUNBCD[1] * 60) + (addressUNBCD[2] + 4)) / 4;
			TOCEntry[0] = tobcd((index < 100) ? index : 99);
		}
		else
		{
			TOCEntry[0] = tobcd(1);
		}

		return TOCEntry;
	}
	if (track == CDEmuTOC_ENDOFDISC)
	{
		if (cdimgLBA >= cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTOC->LastTrack].Address))
		{
			bprintf(0, _T("END OF DISC: curr.lba %06d end lba: %06d\n"), cdimgLBA, cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTOC->LastTrack].Address));
			TOCEntry[0] = 1;
		}

		return TOCEntry;
	}

	track = bcd(track);
	if (track >= cdimgTOC->FirstTrack - 1 && track <= cdimgTOC->LastTrack)
	{
		TOCEntry[0] = cdimgTOC->TrackData[track - 1].Address[1];
		TOCEntry[1] = cdimgTOC->TrackData[track - 1].Address[2];
		TOCEntry[2] = cdimgTOC->TrackData[track - 1].Address[3];
		TOCEntry[3] = cdimgTOC->TrackData[track - 1].Control >> 4;
	}

	// dprintf(_T("    track %02i - %02x:%02x:%02x\n"), track, TOCEntry[0], TOCEntry[1], TOCEntry[2]);

	return TOCEntry;
}

static UINT8* cdimgReadQChannel()
{
	// Q channel format
	// byte 0: 41 = data, 1 = cdda ( flags described at https://en.wikipedia.org/wiki/Compact_Disc_subcode )
	// track, index, M rel, S rel, F rel, M to start, S to start, F to start, 0, CRC, CRC
	// if index is 0, MSF rel counts down to next track

	static UINT8 QChannelData[8];

	switch (CDEmuStatus)
	{
		case reading:
		case playing:
		{
			if (QChannel != NULL) { // .CCD/.SUB
				QChannelData[0] = QChannel[cdimgLBA].track;

				QChannelData[1] = QChannel[cdimgLBA].MSFrel.M;
				QChannelData[2] = QChannel[cdimgLBA].MSFrel.S;
				QChannelData[3] = QChannel[cdimgLBA].MSFrel.F;

				QChannelData[4] = QChannel[cdimgLBA].MSFrel.M;
				QChannelData[5] = QChannel[cdimgLBA].MSFrel.S;
				QChannelData[6] = QChannel[cdimgLBA].MSFrel.F;

				QChannelData[7] = QChannel[cdimgLBA].Control;
			} else { // .BIN/.ISO
				const UINT8* AddressAbs = cdimgLBAToMSF(cdimgLBA);
				const UINT8* AddressRel = cdimgLBAToMSF(cdimgLBA - cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack].Address));

				QChannelData[0] = cdimgTOC->TrackData[cdimgTrack].TrackNumber;

				QChannelData[1] = AddressAbs[1];
				QChannelData[2] = AddressAbs[2];
				QChannelData[3] = AddressAbs[3];

				QChannelData[4] = AddressRel[1];
				QChannelData[5] = AddressRel[2];
				QChannelData[6] = AddressRel[3];

				QChannelData[7] = cdimgTOC->TrackData[cdimgTrack].Control;
			}

			// dprintf(_T("    Q %02x %02x %02x:%02x:%02x %02x:%02x:%02x\n"), QChannel[cdimgLBA].track, QChannel[cdimgLBA].index, QChannel[cdimgLBA].MSFrel.M, QChannel[cdimgLBA].MSFrel.S, QChannel[cdimgLBA].MSFrel.F, QChannel[cdimgLBA].MSFabs.M, QChannel[cdimgLBA].MSFabs.S, QChannel[cdimgLBA].MSFabs.F);

			break;
		}
		case paused:
			break;

		default:
			memset(QChannelData, 0, sizeof(QChannelData));
	}

	return QChannelData;
}

static int cdimgGetSoundBuffer(short* buffer, int samples)
{

#define CLIP(A) ((A) < -0x8000 ? -0x8000 : (A) > 0x7fff ? 0x7fff : (A))

	if (CDEmuStatus != playing) {
		memset(cdimgOutputbuffer, 0x00, cdimgOUT_SIZE * 2 * sizeof(short));
		return 0;
	}

	cdimgSamples += samples;
	while (cdimgSamples > (44100 / CD_FRAMES_SECOND))
	{
		cdimgSamples -= (44100 / CD_FRAMES_SECOND);
		cdimgLBA++;

/*		if (cdimgFile == NULL) // play next track?  bad idea. -dink
			if (cdimgLBA >= cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack + 1].Address))
				cdimgPlayLBA(cdimgLBA); */
	}

#if 0
	extern int counter;
	if (counter) {
		const UINT8* displayaddress = dinkLBAToMSF(cdimgLBA);
		dprintf(_T("  index  %02i:%02i:%02i"), displayaddress[1], displayaddress[2], displayaddress[3]);
		INT32 endt = cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack + 1 /* next track */].Address);
		const UINT8* displayaddressend = dinkLBAToMSF(endt);
		dprintf(_T("    end  %02i:%02i:%02i\n"), displayaddressend[1], displayaddressend[2], displayaddressend[3]);
	}
#endif

	bool is_chd = (cdimgTOC && cdimgTOC->ImageType == CD_TYPE_CHD);

	if (!is_chd && cdimgFile == NULL) { // restart play if fileptr lost
		bprintf(0, _T("CDDA file pointer lost, re-starting @ %d!\n"), cdimgLBA);
		if (cdimgLBA < cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack + 1].Address))
			cdimgPlayLBA(cdimgLBA);
	}

	if (!is_chd && cdimgFile == NULL) { // restart failed (really?) - time to give up.
		cdimgStop();
		return 0;
	}

	if (cdimgLBA >= cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack + 1 /* next track */].Address)) {
		bprintf(0, _T("End of audio track %d reached!! stopping.\n"), cdimgTrack + 1);
		cdimgStop();
		return 0;
	}

	if ((cdimgOutputPosition + samples) >= cdimgOutputbufferSize)
	{
		short* src = cdimgOutputbuffer + cdimgOutputPosition * 2;
		short* dst = buffer;

		for (int i = (cdimgOutputbufferSize - cdimgOutputPosition) * 2 - 1; i > 0; )
		{
			short tmpsrc;
			tmpsrc = BURN_ENDIAN_SWAP_INT16(src[i]);
			dst[i] = CLIP(tmpsrc + dst[i]); i--;
			tmpsrc = BURN_ENDIAN_SWAP_INT16(src[i]);
			dst[i] = CLIP(tmpsrc + dst[i]); i--;
		}

		buffer += (cdimgOutputbufferSize - cdimgOutputPosition) * 2;
		samples -= (cdimgOutputbufferSize - cdimgOutputPosition);

		cdimgOutputPosition = 0;
		if (is_chd) {
			// CHD: decompress the next batch of sectors and repack as LE.
			INT32 sectors_to_read = (cdimgOUT_SIZE * 4) / 2352;
			INT32 read_count = cdimgChdFillAudioBuffer(cdimgAudioFilePos, sectors_to_read);
			cdimgAudioFilePos += read_count;
			cdimgOutputbufferSize = read_count * (2352 / 4);
			if (cdimgOutputbufferSize <= 0)
				cdimgStop();
		} else {
			if ((cdimgOutputbufferSize = fread(cdimgOutputbuffer, 4, cdimgOUT_SIZE, cdimgFile)) <= 0)
				cdimgStop();
		}
	}

	if ((cdimgOutputPosition + samples) < cdimgOutputbufferSize)
	{
		short* src = cdimgOutputbuffer + cdimgOutputPosition * 2;
		short* dst = buffer;

		for (int i = samples * 2 - 1; i > 0; )
		{
			short tmpsrc;
			tmpsrc = BURN_ENDIAN_SWAP_INT16(src[i]);
			dst[i] = CLIP(tmpsrc + dst[i]); i--;
			tmpsrc = BURN_ENDIAN_SWAP_INT16(src[i]);
			dst[i] = CLIP(tmpsrc + dst[i]); i--;
		}

		cdimgOutputPosition += samples;
	}

	return 0;

#undef CLIP

}

static INT32 cdimgScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_VOLATILE) {
		SCAN_VAR(CDEmuStatus);
		SCAN_VAR(cdimgTrack);
		SCAN_VAR(cdimgLBA);

		SCAN_VAR(cdimgOutputPosition);
		SCAN_VAR(cdimgSamples);
		SCAN_VAR(cdimgOutputbufferSize);
		SCAN_VAR(cdimgAudioFilePos);
	}

	if (nAction & ACB_WRITE && nAction & ACB_RUNAHEAD) { // run-ahead system state load
		re_sync = 1;
	}

	if (nAction & ACB_WRITE && ~nAction & ACB_RUNAHEAD) {
		cdimgCloseFile();
	}

	return 0;
}

/**
 * see src/burner/misc.cpp
 */
TCHAR* ExtractFilename(TCHAR* fullname)
{
	TCHAR* filename = fullname + _tcslen(fullname);

	do {
		filename--;
	} while (filename >= fullname && *filename != _T('\\') && *filename != _T('/') && *filename != _T(':'));

	return filename;
}

TCHAR* LabelCheck(TCHAR* s, TCHAR* pszLabel)
{
	INT32 nLen;
	if (s == NULL) {
		return NULL;
	}
	if (pszLabel == NULL) {
		return NULL;
	}
	nLen = _tcslen(pszLabel);

	SKIP_WS(s);													// Skip whitespace

	if (_tcsncmp(s, pszLabel, nLen)){							// Doesn't match
		return NULL;
	}
	return s + nLen;
}

INT32 QuoteRead(TCHAR** ppszQuote, TCHAR** ppszEnd, TCHAR* pszSrc)	// Read a (quoted) string from szSrc and poINT32 to the end
{
	static TCHAR szQuote[QUOTE_MAX];
	TCHAR* s = pszSrc;
	TCHAR* e;

	// Skip whitespace
	SKIP_WS(s);

	e = s;

	if (*s == _T('\"')) {										// Quoted string
		s++;
		e++;
		// Find end quote
		FIND_QT(e);
		_tcsncpy(szQuote, s, e - s);
		// Zero-terminate
		szQuote[e - s] = _T('\0');
		e++;
	} else {													// Non-quoted string
		// Find whitespace
		FIND_WS(e);
		_tcsncpy(szQuote, s, e - s);
		// Zero-terminate
		szQuote[e - s] = _T('\0');
	}

	if (ppszQuote) {
		*ppszQuote = szQuote;
	}
	if (ppszEnd)	{
		*ppszEnd = e;
	}

	return 0;
}

TCHAR *FileExt(TCHAR *str)
{
	TCHAR *dot = strrchr(str, _T('.'));

	return (dot) ? StrLower(dot) : str;
}

bool IsFileExt(TCHAR *str, TCHAR *ext)
{
	return (_tcsicmp(ext, FileExt(str)) == 0);
}

TCHAR *StrReplace(TCHAR *str, TCHAR find, TCHAR replace)
{
	INT32 length = _tcslen(str);

	for (INT32 i = 0; i < length; i++) {
		if (str[i] == find) str[i] = replace;
	}

	return str;
}

// StrLower() - leaves str untouched, returns modified string
TCHAR *StrLower(TCHAR *str)
{
	static TCHAR szBuffer[256] = _T("");
	INT32 length = _tcslen(str);

	if (length > 255) length = 255;

	for (INT32 i = 0; i < length; i++) {
		if (str[i] >= _T('A') && str[i] <= _T('Z'))
			szBuffer[i] = (str[i] + _T(' '));
		else
			szBuffer[i] = str[i];
	}
	szBuffer[length] = 0;

	return &szBuffer[0];
}

/**
 * see src/intf/cd/cd_interface.cpp
 */

INT32 CDEmuInit() {
	INT32 nRet;
	CDEmuStatus = idle;
	if ((nRet = cdimgInit()) == 0) {
		bCDEmuOkay = true;
	}
	return nRet;
}
INT32 CDEmuExit() {
	if (!bCDEmuOkay) {
		return 1;
	}
	bCDEmuOkay = false;
	return cdimgExit();
}
INT32 CDEmuStop() {
	if (!bCDEmuOkay) {
		return 1;
	}
	return cdimgStop();
}
INT32 CDEmuPlay(UINT8 M, UINT8 S, UINT8 F) {
	if (!bCDEmuOkay) {
		return 1;
	}
	return cdimgPlay(M, S, F);
}
INT32 CDEmuLoadSector(INT32 LBA, char* pBuffer) {
	if (!bCDEmuOkay) {
		return 0;
	}
	return cdimgLoadSector(LBA, pBuffer);
}
UINT8* CDEmuReadTOC(INT32 track) {
	if (!bCDEmuOkay) {
		return NULL;
	}
	return cdimgReadTOC(track);
}
UINT8* CDEmuReadQChannel() {
	if (!bCDEmuOkay) {
		return NULL;
	}
	return cdimgReadQChannel();
}
INT32 CDEmuGetSoundBuffer(INT16* buffer, INT32 samples) {
	if (!bCDEmuOkay) {
		return 1;
	}
	return cdimgGetSoundBuffer(buffer, samples);
}
INT32 CDEmuScan(INT32 nAction, INT32 *pnMin)
{
	if (!bCDEmuOkay) {
		return 1;
	}
	return cdimgScan(nAction, pnMin);
}