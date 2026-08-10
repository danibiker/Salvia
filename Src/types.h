#ifndef _TYPES_H
#define _TYPES_H

#include <stdint.h>

#ifdef _WIN32
#define PATHSEP '\\'
#else
#define PATHSEP '/'
#endif

typedef uint8_t	 Uint8;
typedef int8_t	 Sint8;
typedef uint16_t Uint16;
typedef int16_t	 Sint16;
typedef uint32_t Uint32;
typedef int32_t	 Sint32;

typedef int8_t     int8;
typedef int16_t    int16;
typedef int32_t   int32;
typedef uint8_t   uint8;
typedef uint16_t  uint16;
typedef uint32_t  uint32;

#endif
