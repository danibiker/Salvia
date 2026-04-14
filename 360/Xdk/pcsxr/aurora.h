#define AURORA_LAUNCHDATA_APPID					'AUOA'
#define AURORA_LAUNCHDATA_ROM_FUNCID			'ROMS'
#define AURORA_LAUNCHDATA_ROM_VERSION			2

#include <string>

typedef struct _AURORA_LAUNCHDATA_ROM_V1 {
	DWORD ApplicationId;						//   AURORA_LAUNCHDATA_APPID
	DWORD FunctionId;							//   AURORA_LAUNCHDATA_ROM_FUNCID
	DWORD FunctionVersion;						//   AURORA_LAUNCHDATA_ROM_VERSION
	CHAR SystemPath[0x40];						//   /System/Harddisk0/Parition0
	CHAR RelativePath[ 0x104 ];					//   /Emulators/Snes9x/Roms/
	CHAR Exectutable[ 0x28 ];					//   ChronoTrigger.zip
	CHAR Reserved[0x100];						//   Reserved for future use
} AURORA_LAUNCHDATA_ROM_V1, *PAURORA_LAUNCH_DATA_ROM_V1;

typedef struct _AURORA_LAUNCHDATA_ROM_V2 {
	DWORD ApplicationId;						//   AURORA_LAUNCHDATA_APPID
	DWORD FunctionId;							//   AURORA_LAUNCHDATA_ROM_FUNCID
	DWORD FunctionVersion;						//   AURORA_LAUNCHDATA_ROM_VERSION
	CHAR SystemPath[0x40];						//   /System/Harddisk0/Parition0
	CHAR RelativePath[ 0x104 ];					//   /Emulators/Snes9x/Roms/
	CHAR Exectutable[ 0x40 ];					//   ChronoTrigger.zip
	CHAR Reserved[0x100];						//   Reserved for future use
} AURORA_LAUNCHDATA_ROM_V2, *PAURORA_LAUNCH_DATA_ROM_V2;

typedef struct _STRING 
{
    USHORT Length;
    USHORT MaximumLength;
    PCHAR Buffer;
} STRING, *PSTRING;

#ifdef __cplusplus
extern "C" {
#endif
VOID RtlInitAnsiString(PSTRING DestinationString, PCHAR SourceString);	
HRESULT ObDeleteSymbolicLink(PSTRING SymbolicLinkName);
HRESULT ObCreateSymbolicLink(PSTRING SymbolicLinkName, PSTRING DeviceName);
#ifdef __cplusplus
}
#endif

HRESULT xbox_io_mount(const char* szDrive, char* szDevice)
{
	STRING DeviceName, LinkName;
	CHAR szDestinationDrive[255];
	sprintf_s(szDestinationDrive, 255, "\\??\\%s", szDrive);
	RtlInitAnsiString(&DeviceName, szDevice);
	RtlInitAnsiString(&LinkName, szDestinationDrive);
	ObDeleteSymbolicLink(&LinkName);
	return (HRESULT)ObCreateSymbolicLink(&LinkName, &DeviceName);
} 