#include <xtl.h>

#if 1
#define memcpy XMemCpy
#define memset XMemSet
#endif

#if 1
#define malloc(size)	XPhysicalAlloc(size, MAXULONG_PTR, 0,PAGE_READWRITE)
#define free(adress)	XPhysicalFree(adress)
#else
#define malloc(size) VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)
#define free(adress) do { VirtualFree((adress), 0, MEM_RELEASE); } while (0)
#endif