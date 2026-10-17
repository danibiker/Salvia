#pragma once

#define MAXPATHLEN 4096
#define PACKAGE_VERSION "Pcsx-r"

#ifndef strcasecmp
#define strcasecmp  _stricmp
#endif

#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

#ifndef strtok_r
#define strtok_r strtok_s
#endif

#ifndef snprintf
#define snprintf _snprintf
#endif

#define __attribute__(x)


