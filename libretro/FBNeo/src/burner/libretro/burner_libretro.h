#ifndef _BURNER_LIBRETRO_H
#define _BURNER_LIBRETRO_H

#include "gameinp.h"
#include "cd_interface.h"

char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int /*nOutSize*/);
extern void InpDIPSWResetDIPs (void);
extern TCHAR szAppBurnVer[16];

#ifdef _XBOX

#ifndef ANSIToTCHAR
    #define ANSIToTCHAR(str, foo, bar) (str)
#endif

#include <compat/msvc.h>
#include <tchar.h>
#ifndef _tcscmp
    #ifdef _UNICODE
        #define _tcscmp wcscmp
    #else
        #define _tcscmp strcmp
    #endif
#endif
#endif
extern INT32 DrvExit(); 

#endif
