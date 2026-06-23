#include "burner.h"

#define NUM_LANGUAGES		12
#define MAX_NODES			1024
#define MAX_ACTIVE_PATCHES	1024

static HWND hIpsDlg			= NULL;
static HWND hParent			= NULL;
static HWND hIpsList		= NULL;

int nIpsSelectedLanguage		= 0;
static TCHAR szFullName[1024];
static TCHAR szLanguages[NUM_LANGUAGES][32];
static TCHAR szLanguageCodes[NUM_LANGUAGES][6];
 

TCHAR szIpsActivePatches[MAX_ACTIVE_PATCHES][MAX_PATH];
 
static TCHAR* GameIpsConfigName()
{
	// Return the path of the config file for this game
	static TCHAR szName[32];
	_stprintf(szName, _T("config\\ips\\%s.ini"), BurnDrvGetText(DRV_NAME));
	return szName;
}

int GetIpsNumPatches()
{
	WIN32_FIND_DATA wfd;
	HANDLE hSearch;
	TCHAR szFilePath[MAX_PATH];
	int Count = 0;

	_stprintf(szFilePath, _T("%s%s\\"), szAppIpsPath, BurnDrvGetText(DRV_NAME));
	_tcscat(szFilePath, _T("*.dat"));
	
	hSearch = FindFirstFile(szFilePath, &wfd);

	if (hSearch != INVALID_HANDLE_VALUE) {
		int Done = 0;

		while (!Done ) {
			Count++;
			Done = !FindNextFile(hSearch, &wfd);
		}

		FindClose(hSearch);
	}

	return Count;
}

static TCHAR* GetPatchDescByLangcode(FILE* fp, int nLang)
{
	TCHAR* result = NULL;
	char* desc = NULL;
	char langtag[8];

	sprintf(langtag, "[%s]", TCHARToANSI(szLanguageCodes[nLang], NULL, 0));
	
	fseek(fp, 0, SEEK_SET);

	while (!feof(fp))
	{
		char s[4096];

		if (fgets(s, sizeof s, fp) != NULL)
		{
			if (strncmp(langtag, s, strlen(langtag)) != 0)
				continue;

			while (fgets(s, sizeof s, fp) != NULL)
			{
				char* p;

				if (*s == '[')
				{
					if (desc)
					{
						result = tstring_from_utf8(desc);
						free(desc);
						return result;
					}
					else
						return NULL;
				}

				for (p = s; *p; p++)
				{
					if (*p == '\r' || *p == '\n')
					{
						*p = '\0';
						break;
					}
				}
				
				if (desc)
				{
					char* p1;
					int len = strlen(desc);

					len += strlen(s) + 2;
					p1 = (char*)malloc(len + 1);
					sprintf(p1, "%s\r\n%s", desc, s);
					free(desc);
					desc = p1;
				}
				else
				{
					desc = (char*)malloc(strlen(s) + 1);
					if (desc != NULL)
						strcpy(desc, s);
				}
			}
		}
	}

	if (desc)
	{
		result = tstring_from_utf8(desc);
		free(desc);
		return result;
	}
	else
		return NULL;
}

 

int GetIpsNumActivePatches()
{
	int nActivePatches = 0;
	
	for (int i = 0; i < MAX_ACTIVE_PATCHES; i++) {
		if (_tcsicmp(szIpsActivePatches[i], _T(""))) nActivePatches++;
	}
	
	return nActivePatches;
}

void LoadIpsActivePatches()
{
	for (int i = 0; i < MAX_ACTIVE_PATCHES; i++) {
		_stprintf(szIpsActivePatches[i], _T(""));
	}
	
	FILE* fp = _tfopen(GameIpsConfigName(), _T("rt"));
	TCHAR szLine[MAX_PATH];
	int nActivePatches = 0;
	
	if (fp) {
		while (_fgetts(szLine, sizeof(szLine), fp)) {
			int nLen = _tcslen(szLine);
			
			// Get rid of the linefeed at the end
			if (szLine[nLen - 1] == 10) {
				szLine[nLen - 1] = 0;
				nLen--;
			}
			
			if (!_tcsnicmp(szLine, _T("//"), 2)) continue;
			if (!_tcsicmp(szLine, _T(""))) continue;
			
			_stprintf(szIpsActivePatches[nActivePatches], _T("%s%s\\%s"), szAppIpsPath, BurnDrvGetText(DRV_NAME), szLine);
			nActivePatches++;
		}		
		
		fclose(fp);
	}
}
  
 
 
// Game patching

#define UTF8_SIGNATURE	"\xef\xbb\xbf"
#define IPS_SIGNATURE	"PATCH"
#define IPS_TAG_EOF	"EOF"
#define IPS_EXT		".ips"

#define BYTE3_TO_UINT(bp) \
     (((unsigned int)(bp)[0] << 16) & 0x00FF0000) | \
     (((unsigned int)(bp)[1] << 8) & 0x0000FF00) | \
     ((unsigned int)(bp)[2] & 0x000000FF)

#define BYTE2_TO_UINT(bp) \
    (((unsigned int)(bp)[0] << 8) & 0xFF00) | \
    ((unsigned int) (bp)[1] & 0x00FF)
    
bool bDoIpsPatch = FALSE;

static void PatchFile(const char* ips_path, UINT8* base)
{
	char buf[6];
	FILE* f = NULL;
	int Offset, Size;
	UINT8* mem8 = NULL;

	if (NULL == (f = fopen(ips_path, "rb")))
		return;

	memset(buf, 0, sizeof buf);
	fread(buf, 1, 5, f);
	if (strcmp(buf, IPS_SIGNATURE)) {
		return;
	} else {
		UINT8 ch = 0;
		int bRLE = 0;
		while (!feof(f)) {
			// read patch address offset
			fread(buf, 1, 3, f);
			buf[3] = 0;
			if (strcmp(buf, IPS_TAG_EOF) == 0)
				break;

			Offset = BYTE3_TO_UINT(buf);

			// read patch length
			fread(buf, 1, 2, f);
			Size = BYTE2_TO_UINT(buf);

			bRLE = (Size == 0);
			if (bRLE) {
				fread(buf, 1, 2, f);
				Size = BYTE2_TO_UINT(buf);
				ch = fgetc(f);
			}

			while (Size--) {
				mem8 = base + Offset;
				Offset++;
				*mem8 = bRLE ? ch : fgetc(f);
			}
		}
	}

	fclose(f);
}

static void DoPatchGame(const char* patch_name, char* game_name, UINT8* base)
{
	char s[MAX_PATH];
	char* p = NULL;
	char* rom_name = NULL;
	char* ips_name = NULL;
	FILE* fp = NULL;
	unsigned long nIpsSize;

	if ((fp = fopen(patch_name, "rb")) != NULL) {
		// get ips size
		fseek(fp, 0, SEEK_END);
		nIpsSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		while (!feof(fp)) {
			if (fgets(s, sizeof s, fp) != NULL) {
				p = s;

				// skip UTF-8 sig
				if (strncmp(p, UTF8_SIGNATURE, strlen(UTF8_SIGNATURE)) == 0)
					p += strlen(UTF8_SIGNATURE);

				if (p[0] == '[')	// '['
					break;

				rom_name = strtok(p, " \t\r\n");
				if (!rom_name)
					continue;
				if (*rom_name == '#')
					continue;
				if (_stricmp(rom_name, game_name))
					continue;

				ips_name = strtok(NULL, " \t\r\n");
				if (!ips_name)
					continue;

				// skip CRC check
				strtok(NULL, "\r\n");

				char ips_path[MAX_PATH];
				char ips_dir[MAX_PATH];
				TCHARToANSI(szAppIpsPath, ips_dir, sizeof(ips_dir));

				if (strchr(ips_name, '\\')) {
					// ips in parent's folder
					sprintf(ips_path, "%s\\%s%s", ips_dir, ips_name, IPS_EXT);
				} else {
					sprintf(ips_path, "%s%s\\%s%s", ips_dir, BurnDrvGetTextA(DRV_NAME), ips_name, IPS_EXT);
				}

				PatchFile(ips_path, base);
			}
		}
		fclose(fp);
	}
}

void IpsApplyPatches(UINT8* base, char* rom_name)
{
	char ips_data[MAX_PATH];
	
	int nActivePatches = GetIpsNumActivePatches();
	
	for (int i = 0; i < nActivePatches; i++) {
		memset(ips_data, 0, MAX_PATH);
		TCHARToANSI(szIpsActivePatches[i], ips_data, sizeof(ips_data));
		DoPatchGame(ips_data, rom_name, base);
	}
}

void IpsPatchExit()
{
	bDoIpsPatch = FALSE;
}
