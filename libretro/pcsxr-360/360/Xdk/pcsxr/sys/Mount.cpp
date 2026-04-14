#include <xtl.h>
#include "Mount.h"
#include <string>
#include "kernel.h"

extern "C"{
	HRESULT XamContentOpenFile(u32, char*, char*, u32, u32, u32, u32*);
	HRESULT XamContentClose(char*, u32*);
	HRESULT XamContentFlush(char*, u32*);
}


//proto
void (*_XamLoaderLaunchTitle)(const char *, u32);
u32 (*_XamContentOpenFile)(u32, char*, char*,u32,u32,u32,u32*);//last 3 u32 are things like xoverlapped ptrs
u32 (*_XamContentClose)(char*,u32*);

#define XAMCONTENTOPENFILE_ORD (u32)0x269
#define XAMCONTENTCLOSE_ORD (u32)0x25a

u32 resolveFunct(char* modname, u32 ord)
{
	HANDLE ptr32=0;
	UINT32 ret=0, ptr2=0;
	ret = XexGetModuleHandle(modname, &ptr32); //xboxkrnl.exe xam.dll?
	if(ret == 0)
	{
		ret = XexGetProcedureAddress(ptr32, ord, &ptr2 );
		if(ptr2 != 0)
			return ptr2;
	}
	return 0; // function not found
}

// returns 0 on success, creates symbolic link on it's own to the new szDrive
unsigned int XContentMountEX(CHAR* szDrive, CHAR* szDevice, CHAR* szPath)
{
	u32 ret;
	CHAR szMountPath[MAX_PATH];
	if(_XamContentOpenFile == 0)
	{
		ret = resolveFunct("xam.xex", XAMCONTENTOPENFILE_ORD); 
		_XamContentOpenFile = (u32 (__cdecl *)(u32, char*, char*,u32,u32,u32,u32*))ret;
		if(_XamContentOpenFile == 0)
			return ERROR_INVALID_HANDLE; // well, invalid handle good enough for me...
	}
	sprintf_s(szMountPath,MAX_PATH,"\\??\\%s\\%s",szDevice,szPath);
	return _XamContentOpenFile(0xFE, szDrive, szMountPath, 0x4000003,0,0,0);
}

unsigned int XContentMount(LPCSTR  szDrive, LPCSTR  szPath)
{
	//separe device et path
	std::string str;
	std::string device;
	std::string name;
	int tmp=0;

	str=szPath;
	tmp=str.find(":\\");

	device=str.substr(0,tmp+1);
	name=str.substr(tmp+2);

	return XContentMountEX((char*)szDrive,(char*)device.c_str(),(char*)name.c_str());
}

// returns 0 on success, destorys symbolic link as well
unsigned int XContentUnMount(LPCSTR  szDrive)
{
	USHORT len;
	u32 ret;
	CHAR szMountPath[MAX_PATH];
	if(_XamContentClose == 0) // make sure the funciton is resolved
	{
		ret = resolveFunct("xam.xex", XAMCONTENTCLOSE_ORD);
		_XamContentClose = (u32 (__cdecl *)(char *,u32 *))ret;
		if(_XamContentClose == 0)
			return ERROR_INVALID_HANDLE;
	}
	sprintf_s(szMountPath,MAX_PATH,"\\??\\%s",szDrive);
	return  _XamContentClose(szMountPath,0);
}



// returns 0 on success, destorys symbolic link as well
unsigned int XContentUnMountEX(CHAR* szDrive, CHAR* szDevice, CHAR* szPath)
{
	USHORT len;
	u32 ret;
	CHAR szMountPath[MAX_PATH];
	if(_XamContentClose == 0) // make sure the funciton is resolved
	{
		ret = resolveFunct("xam.xex", XAMCONTENTCLOSE_ORD);
		_XamContentClose = (u32 (__cdecl *)(char *,u32 *))ret;
		if(_XamContentClose == 0)
			return ERROR_INVALID_HANDLE;
	}
	sprintf_s(szMountPath,MAX_PATH,"\\??\\%s",szDrive);
	return  _XamContentClose(szMountPath,0);
}

/*
 * Monte une partion
 * example: Map("drive:","\\device\\blabla\\");
 */
HRESULT Map( CHAR* szDrive, CHAR* szDevice )
{
	
	CHAR * szSourceDevice;
	CHAR szDestinationDrive[MAX_PATH];
	USHORT len;
	szSourceDevice = szDevice;

	sprintf_s(szDestinationDrive,MAX_PATH,"\\??\\%s",szDrive);
	len = (USHORT)strlen(szSourceDevice);
	STRING DeviceName =
	{
		len,
		len + 1,
		szSourceDevice
	};
	len = (USHORT)strlen(szDestinationDrive);
	STRING LinkName =
	{
		len,
		len + 1,
		szDestinationDrive
	};
	return ( HRESULT )ObCreateSymbolicLink(&LinkName, &DeviceName);
}
/*
 * Demonte une partion
 * example: unMap("drive:");
 */
HRESULT unMap( CHAR* szDrive )
{
	CHAR szDestinationDrive[MAX_PATH];
	sprintf_s( szDestinationDrive,"\\??\\%s", szDrive );

	STRING LinkName =
	{
		strlen(szDestinationDrive),
		strlen(szDestinationDrive) + 1,
		szDestinationDrive
	};

	return ( HRESULT )ObDeleteSymbolicLink( &LinkName );
}



// returns 0 on success, creates symbolic link on it's own to the new szDrive
HRESULT mountCon(CHAR* szDrive, const CHAR* szDevice, const CHAR* szPath) {

   CHAR szMountPath[MAX_PATH];
   sprintf_s(szMountPath, MAX_PATH, "\\??\\%s\\%s", szDevice, szPath);
   return XamContentOpenFile(0xFE, szDrive, szMountPath, 0x4000043, 0, 0, 0);
}

// returns 0 on success, destorys symbolic link as well
HRESULT unmountCon(CHAR* szDrive, bool bSaveChanges) {

   CHAR szMountPath[MAX_PATH];
   sprintf_s(szMountPath, MAX_PATH, "\\??\\%s", szDrive);

   if(bSaveChanges)
		XamContentFlush(szMountPath, 0);

   return XamContentClose(szMountPath, 0);
}