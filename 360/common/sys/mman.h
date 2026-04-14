#define WORDS_BIGENDIAN 1
#define __BIG_ENDIAN__
#define __ppc__
#define inline __forceinline

#define __attribute__(x)
#define MAXPATHLEN 4096

#define PACKAGE_VERSION "1.1"


#define PROT_WRITE 
#define PROT_READ
#define MAP_PRIVATE
#define MAP_ANONYMOUS

#if 0 //virtual

#define mmap(start, length, prot, flags, fd, offset) \
	((unsigned char *)VirtualAlloc(NULL, (length), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))

#define munmap(start, length) do { VirtualFree((start), (length), MEM_RELEASE); } while (0)
#else

#define mmap(start, length, prot, flags, fd, offset) \
	((unsigned char *)XPhysicalAlloc(length * sizeof(void*), MAXULONG_PTR, 0,PAGE_READWRITE | MEM_LARGE_PAGES))

#define munmap(start, length) do { XPhysicalFree(start); } while (0)

#endif