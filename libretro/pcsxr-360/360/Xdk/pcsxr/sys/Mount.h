#pragma once


HRESULT Map( CHAR* szDrive, CHAR* szDevice );
HRESULT mountCon(CHAR* szDrive, const CHAR* szDevice, const CHAR* szPath);
HRESULT unmountCon(CHAR* szDrive, bool bSaveChanges);


unsigned int XContentMount(LPCSTR  szDrive, LPCSTR  szPath);
unsigned int XContentUnMount(LPCSTR  szDrive);
