#ifndef __MSVC_COMPAT_H
#define __MSVC_COMPAT_H

#if defined(_MSC_VER)
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
#endif

#endif __MSVC_COMPAT_H